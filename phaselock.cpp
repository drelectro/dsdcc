///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////
//#include "pch.h"
#define NOMINMAX

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <stdio.h>
#include <float.h>
#include "phaselock.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace DSDcc
{

// Construct phase-locked loop.
PhaseLock::PhaseLock(float freq, float bandwidth, float minsignal)
{
    /*
     * This is a type-2, 4th order phase-locked loop.
     *
     * Open-loop transfer function:
     *   G(z) = K * (z - q1) / ((z - p1) * (z - p2) * (z - 1) * (z - 1))
     *   K  = 3.788 * (bandwidth * 2 * Pi)**3
     *   q1 = exp(-0.1153 * bandwidth * 2*Pi)
     *   p1 = exp(-1.146 * bandwidth * 2*Pi)
     *   p2 = exp(-5.331 * bandwidth * 2*Pi)
     *
     * I don't understand what I'm doing; hopefully it will work.
     */

    // Set min/max locking frequencies.
    m_minfreq = (freq - bandwidth) * 2.0 * M_PI;
    m_maxfreq = (freq + bandwidth) * 2.0 * M_PI;

    // Set valid signal threshold.
    m_minsignal  = minsignal;
    m_lock_delay = int(1.0 / bandwidth);
    //m_lock_delay = 1;
    m_lock_cnt   = 0;
    m_psin = 0.0;
    m_pcos = 1.0;

    // Create 2nd order filter for I/Q representation of phase error.
    // Filter has two poles, unit DC gain.
    double p1 = exp(-1.146 * bandwidth * 2.0 * M_PI);
    double p2 = exp(-5.331 * bandwidth * 2.0 * M_PI);
    m_phasor_a1 = - p1 - p2;
    m_phasor_a2 = p1 * p2;
    m_phasor_b0 = 1 + m_phasor_a1 + m_phasor_a2;

    // Create loop filter to stabilize the loop.
    double q1 = exp(-0.1153 * bandwidth * 2.0 * M_PI);
    m_loopfilter_b0 = 0.62 * bandwidth * 2.0 * M_PI;
    m_loopfilter_b1 = - m_loopfilter_b0 * q1;

    // After the loop filter, the phase error is integrated to produce
    // the frequency. Then the frequency is integrated to produce the phase.
    // These integrators form the two remaining poles, both at z = 1.

    // Initialize frequency and phase.
    m_freq  = freq * 2.0 * M_PI;
    m_phase = 0;

    m_phasor_i1 = 0;
    m_phasor_i2 = 0;
    m_phasor_q1 = 0;
    m_phasor_q2 = 0;
    m_loopfilter_x1 = 0;

    // Initialize PPS generator.
    m_sample_cnt    = 0;
}


void PhaseLock::configure(float freq, float bandwidth, float minsignal)
{
    /*
     * This is a type-2, 4th order phase-locked loop.
     *
     * Open-loop transfer function:
     *   G(z) = K * (z - q1) / ((z - p1) * (z - p2) * (z - 1) * (z - 1))
     *   K  = 3.788 * (bandwidth * 2 * Pi)**3
     *   q1 = exp(-0.1153 * bandwidth * 2*Pi)
     *   p1 = exp(-1.146 * bandwidth * 2*Pi)
     *   p2 = exp(-5.331 * bandwidth * 2*Pi)
     *
     * I don't understand what I'm doing; hopefully it will work.
     */

    // Set min/max locking frequencies.
    m_minfreq = (freq - bandwidth) * 2.0 * M_PI;
    m_maxfreq = (freq + bandwidth) * 2.0 * M_PI;

    // Set valid signal threshold.
    m_minsignal  = minsignal;
    m_lock_delay = int(1.0 / bandwidth);
    //m_lock_delay = 1;
    m_lock_cnt   = 0;

    fprintf(stderr, "PhaseLock::configure: freq: %f bandwidth: %f minsignal: %f, m_lock_delay: %d\n",
            freq,
            bandwidth,
            minsignal,
            m_lock_delay);

    // Create 2nd order filter for I/Q representation of phase error.
    // Filter has two poles, unit DC gain.
    double p1 = exp(-1.146 * bandwidth * 2.0 * M_PI);
    double p2 = exp(-5.331 * bandwidth * 2.0 * M_PI);
    m_phasor_a1 = - p1 - p2;
    m_phasor_a2 = p1 * p2;
    m_phasor_b0 = 1 + m_phasor_a1 + m_phasor_a2;

    // Create loop filter to stabilize the loop.
    double q1 = exp(-0.1153 * bandwidth * 2.0 * M_PI);
    m_loopfilter_b0 = 0.62 * bandwidth * 2.0 * M_PI;
    m_loopfilter_b1 = - m_loopfilter_b0 * q1;

    // After the loop filter, the phase error is integrated to produce
    // the frequency. Then the frequency is integrated to produce the phase.
    // These integrators form the two remaining poles, both at z = 1.

    // Initialize frequency and phase.
    m_freq  = freq * 2.0 * M_PI;
    m_phase = 0;

    m_phasor_i1 = 0;
    m_phasor_i2 = 0;
    m_phasor_q1 = 0;
    m_phasor_q2 = 0;
    m_loopfilter_x1 = 0;

    // Initialize PPS generator.
    m_sample_cnt    = 0;
}


// Process samples. Bufferized version
void PhaseLock::process(const std::vector<float>& samples_in, std::vector<float>& samples_out)
{
    unsigned int n = samples_in.size();

    samples_out.resize(n);
    float pilot_level = FLT_MAX;

    for (unsigned int i = 0; i < n; i++) {

        // Generate locked pilot tone.
        float psin = sin(m_phase);
        float pcos = cos(m_phase);

        // Generate double-frequency output.
        // sin(2*x) = 2 * sin(x) * cos(x)
        samples_out[i] = 2 * psin * pcos;

        // Multiply locked tone with input.
        float x = samples_in[i];
        float phasor_i = psin * x;
        float phasor_q = pcos * x;

        // Run IQ phase error through low-pass filter.
        phasor_i = m_phasor_b0 * phasor_i
                   - m_phasor_a1 * m_phasor_i1
                   - m_phasor_a2 * m_phasor_i2;
        phasor_q = m_phasor_b0 * phasor_q
                   - m_phasor_a1 * m_phasor_q1
                   - m_phasor_a2 * m_phasor_q2;
        m_phasor_i2 = m_phasor_i1;
        m_phasor_i1 = phasor_i;
        m_phasor_q2 = m_phasor_q1;
        m_phasor_q1 = phasor_q;

        // Convert I/Q ratio to estimate of phase error.
        float phase_err;
        if (phasor_i > std::abs(phasor_q)) {
            // We are within +/- 45 degrees from lock.
            // Use simple linear approximation of arctan.
            phase_err = phasor_q / phasor_i;
        } else if (phasor_q > 0) {
            // We are lagging more than 45 degrees behind the input.
            phase_err = 1;
        } else {
            // We are more than 45 degrees ahead of the input.
            phase_err = -1;
        }

        // Detect pilot level (conservative).
        pilot_level = (std::min)(pilot_level, phasor_i);

        // Run phase error through loop filter and update frequency estimate.
        m_freq += m_loopfilter_b0 * phase_err
                  + m_loopfilter_b1 * m_loopfilter_x1;
        m_loopfilter_x1 = phase_err;

        // Limit frequency to allowable range.
        m_freq = (std::max)(m_minfreq, (std::min)(m_maxfreq, m_freq));

        // Update locked phase.
        m_phase += m_freq;
        if (m_phase > 2.0 * M_PI) {
            m_phase -= 2.0 * M_PI;
        }

        // Update lock status.
        if ((phase_err > -m_minsignal) && (phase_err < m_minsignal))
        {
            if (m_lock_cnt < 2*m_lock_delay)
            {
                m_lock_cnt += 1;
            }
        }
        else
        {
            if (m_lock_cnt > 0)
            {
                m_lock_cnt -= 1;
            }
        }
    }

    // Update sample counter.
    m_sample_cnt += n;
}


// Process samples. Multiple output
void PhaseLock::process(const float& sample_in, float *samples_out)
{
	// Generate locked pilot tone.
	m_psin = sin(m_phase);
	m_pcos = cos(m_phase);

	// Generate output
	processPhase(samples_out);

	// Multiply locked tone with input.
	float x = sample_in;
	float phasor_i = m_psin * x;
	float phasor_q = m_pcos * x;

	// Run IQ phase error through low-pass filter.
	phasor_i = m_phasor_b0 * phasor_i
			   - m_phasor_a1 * m_phasor_i1
			   - m_phasor_a2 * m_phasor_i2;
	phasor_q = m_phasor_b0 * phasor_q
			   - m_phasor_a1 * m_phasor_q1
			   - m_phasor_a2 * m_phasor_q2;
	m_phasor_i2 = m_phasor_i1;
	m_phasor_i1 = phasor_i;
	m_phasor_q2 = m_phasor_q1;
	m_phasor_q1 = phasor_q;

	// Convert I/Q ratio to estimate of phase error.
	float phase_err;
    if (phasor_i > std::abs(phasor_q)) {
		// We are within +/- 45 degrees from lock.
		// Use simple linear approximation of arctan.
		phase_err = phasor_q / phasor_i;
	} else if (phasor_q > 0) {
		// We are lagging more than 45 degrees behind the input.
		phase_err = 1;
	} else {
		// We are more than 45 degrees ahead of the input.
		phase_err = -1;
	}

    // Update lock status.
    if ((phase_err > -m_minsignal) && (phase_err < m_minsignal))
    {
        if (m_lock_cnt < 2*m_lock_delay)
        {
            m_lock_cnt += 1;
        }
    }
    else
    {
        if (m_lock_cnt > 0)
        {
            m_lock_cnt -= 1;
        }
    }

	// Run phase error through loop filter and update frequency estimate.
	m_freq += m_loopfilter_b0 * phase_err
			  + m_loopfilter_b1 * m_loopfilter_x1;
	m_loopfilter_x1 = phase_err;

	// Limit frequency to allowable range.
	m_freq = (std::max)(m_minfreq, (std::min)(m_maxfreq, m_freq));

	// Update locked phase.
	m_phase += m_freq;
	if (m_phase > 2.0 * M_PI)
	{
		m_phase -= 2.0 * M_PI;
	}

    // Update sample counter.
    m_sample_cnt += 1; // n
}

} // namespace DSDcc
