///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
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
#include "pch.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include "dsd_filters.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace DSDcc
{

const float DSDFilters::ngain = 7.423339364f;
const float DSDFilters::nxgain = 15.95930463f;
const float DSDFilters::dmrgain = 6.82973073748f;
const float DSDFilters::dpmrgain = 14.6083498224f;

// DMR filter - DSD original for 4800 baud - root raised cosine alpha=0.2 Ts = 6000 S/s Fc = 48kHz - zero at boundaries
const float DSDFilters::xcoeffs[] =
{  -0.0083649323f, -0.0265444850f, -0.0428141462f, -0.0537571943f,
        -0.0564141052f, -0.0489161045f, -0.0310068662f, -0.0043393881f,
        +0.0275375106f, +0.0595423283f, +0.0857543325f, +0.1003565948f,
        +0.0986944931f, +0.0782804830f, +0.0395670487f, -0.0136691535f,
        -0.0744390415f, -0.1331834575f, -0.1788967208f, -0.2005995448f,
        -0.1889627181f, -0.1378439993f, -0.0454976231f, +0.0847488694f,
        +0.2444859269f, +0.4209222342f, +0.5982295474f, +0.7593684540f,
        +0.8881539892f, +0.9712773915f, +0.9999999166f, +0.9712773915f,
        +0.8881539892f, +0.7593684540f, +0.5982295474f, +0.4209222342f,
        +0.2444859269f, +0.0847488694f, -0.0454976231f, -0.1378439993f,
        -0.1889627181f, -0.2005995448f, -0.1788967208f, -0.1331834575f,
        -0.0744390415f, -0.0136691535f, +0.0395670487f, +0.0782804830f,
        +0.0986944931f, +0.1003565948f, +0.0857543325f, +0.0595423283f,
        +0.0275375106f, -0.0043393881f, -0.0310068662f, -0.0489161045f,
        -0.0564141052f, -0.0537571943f, -0.0428141462f, -0.0265444850f,
        -0.0083649323f, };

// NXDN filter - DSD original for 2400 baud - root raised cosine alpha=0.2 Ts = 3000 S/s Fc = 48 kHz
const float DSDFilters::nxcoeffs[] =
{ +0.031462429f, +0.031747267f, +0.030401148f, +0.027362877f, +0.022653298f,
        +0.016379869f, +0.008737200f, +0.000003302f, -0.009468531f,
        -0.019262057f, -0.028914291f, -0.037935027f, -0.045828927f,
        -0.052119261f, -0.056372283f, -0.058221106f, -0.057387924f,
        -0.053703443f, -0.047122444f, -0.037734535f, -0.025769308f,
        -0.011595336f, +0.004287292f, +0.021260954f, +0.038610717f,
        +0.055550276f, +0.071252765f, +0.084885375f, +0.095646450f,
        +0.102803611f, +0.105731303f, +0.103946126f, +0.097138329f,
        +0.085197939f, +0.068234131f, +0.046586711f, +0.020828821f,
        -0.008239664f, -0.039608255f, -0.072081234f, -0.104311776f,
        -0.134843790f, -0.162160200f, -0.184736015f, -0.201094346f,
        -0.209863285f, -0.209831516f, -0.200000470f, -0.179630919f,
        -0.148282051f, -0.105841323f, -0.052543664f, +0.011020985f,
        +0.083912428f, +0.164857408f, +0.252278939f, +0.344336996f,
        +0.438979335f, +0.534000832f, +0.627109358f, +0.715995947f,
        +0.798406824f, +0.872214756f, +0.935487176f, +0.986548646f,
        +1.024035395f, +1.046939951f, +1.054644241f, +1.046939951f,
        +1.024035395f, +0.986548646f, +0.935487176f, +0.872214756f,
        +0.798406824f, +0.715995947f, +0.627109358f, +0.534000832f,
        +0.438979335f, +0.344336996f, +0.252278939f, +0.164857408f,
        +0.083912428f, +0.011020985f, -0.052543664f, -0.105841323f,
        -0.148282051f, -0.179630919f, -0.200000470f, -0.209831516f,
        -0.209863285f, -0.201094346f, -0.184736015f, -0.162160200f,
        -0.134843790f, -0.104311776f, -0.072081234f, -0.039608255f,
        -0.008239664f, +0.020828821f, +0.046586711f, +0.068234131f,
        +0.085197939f, +0.097138329f, +0.103946126f, +0.105731303f,
        +0.102803611f, +0.095646450f, +0.084885375f, +0.071252765f,
        +0.055550276f, +0.038610717f, +0.021260954f, +0.004287292f,
        -0.011595336f, -0.025769308f, -0.037734535f, -0.047122444f,
        -0.053703443f, -0.057387924f, -0.058221106f, -0.056372283f,
        -0.052119261f, -0.045828927f, -0.037935027f, -0.028914291f,
        -0.019262057f, -0.009468531f, +0.000003302f, +0.008737200f,
        +0.016379869f, +0.022653298f, +0.027362877f, +0.030401148f,
        +0.031747267f, +0.031462429f, };

// DMR filter - root raised cosine alpha=0.7 Ts = 6650 S/s Fc = 48kHz
const float DSDFilters::dmrcoeffs[] =
{0.0301506278, 0.0269200615, 0.0159662432, -0.0013114705, -0.0216605133, -0.0404938748, -0.0528141756, -0.0543747957, -0.0428325003, -0.0186176083, 0.0147202645, 0.0508418571, 0.0816392577, 0.0988113688, 0.0957187780, 0.0691512084, 0.0206194642, -0.0431564563, -0.1107569268, -0.1675773224, -0.1981519842, -0.1889130786, -0.1308939560, -0.0218608492, 0.1325685970, 0.3190962499, 0.5182530574, 0.7070497652, 0.8623526878, 0.9644213921, 1.0000000000, 0.9644213921, 0.8623526878, 0.7070497652, 0.5182530574, 0.3190962499, 0.1325685970, -0.0218608492, -0.1308939560, -0.1889130786, -0.1981519842, -0.1675773224, -0.1107569268, -0.0431564563, 0.0206194642, 0.0691512084, 0.0957187780, 0.0988113688, 0.0816392577, 0.0508418571, 0.0147202645, -0.0186176083, -0.0428325003, -0.0543747957, -0.0528141756, -0.0404938748, -0.0216605133, -0.0013114705, 0.0159662432, 0.0269200615, 0.0301506278};

// dPMR filter - root raised cosine alpha=0.2 Ts = 3325 S/s Fc = 48 kHz - zero at boundaries appears to be slightly better
const float DSDFilters::dpmrcoeffs[] =
{-0.0000983004, 0.0058388841, 0.0119748846, 0.0179185547, 0.0232592816,
        0.0275919612, 0.0305433586, 0.0317982965, 0.0311240307,
        0.0283911865, 0.0235897433, 0.0168387650, 0.0083888763,
        -0.0013831396, -0.0119878087, -0.0228442151, -0.0333082708,
        -0.0427067804, -0.0503756642, -0.0557003599, -0.0581561791,
        -0.0573462646, -0.0530347941, -0.0451732069, -0.0339174991,
        -0.0196350217, -0.0028997157, 0.0155246961, 0.0347134030,
        0.0536202583, 0.0711271166, 0.0861006725, 0.0974542022,
        0.1042112035, 0.1055676660, 0.1009496091, 0.0900625944,
        0.0729301774, 0.0499186839, 0.0217462748, -0.0105250265,
        -0.0455148664, -0.0815673067, -0.1168095612, -0.1492246435,
        -0.1767350726, -0.1972941202, -0.2089805758, -0.2100926829,
        -0.1992367833, -0.1754063031, -0.1380470370, -0.0871052089,
        -0.0230554989, 0.0530929052, 0.1398131936, 0.2351006721,
        0.3365341927, 0.4413570929, 0.5465745033, 0.6490630781,
        0.7456885564, 0.8334261381, 0.9094784589, 0.9713859928,
        1.0171250045, 1.0451886943, 1.0546479089, 1.0451886943,
        1.0171250045, 0.9713859928, 0.9094784589, 0.8334261381,
        0.7456885564, 0.6490630781, 0.5465745033, 0.4413570929,
        0.3365341927, 0.2351006721, 0.1398131936, 0.0530929052,
        -0.0230554989, -0.0871052089, -0.1380470370, -0.1754063031,
        -0.1992367833, -0.2100926829, -0.2089805758, -0.1972941202,
        -0.1767350726, -0.1492246435, -0.1168095612, -0.0815673067,
        -0.0455148664, -0.0105250265, 0.0217462748, 0.0499186839,
        0.0729301774, 0.0900625944, 0.1009496091, 0.1055676660,
        0.1042112035, 0.0974542022, 0.0861006725, 0.0711271166,
        0.0536202583, 0.0347134030, 0.0155246961, -0.0028997157,
        -0.0196350217, -0.0339174991, -0.0451732069, -0.0530347941,
        -0.0573462646, -0.0581561791, -0.0557003599, -0.0503756642,
        -0.0427067804, -0.0333082708, -0.0228442151, -0.0119878087,
        -0.0013831396, 0.0083888763, 0.0168387650, 0.0235897433,
        0.0283911865, 0.0311240307, 0.0317982965, 0.0305433586,
        0.0275919612, 0.0232592816, 0.0179185547, 0.0119748846,
        0.0058388841, -0.0000983004};

DSDFilters::DSDFilters()
{
    for (int i=0; i < NZEROS+1; i++) {
        xv[i] = 0.0f;
    }

    for (int i=0; i < NXZEROS+1; i++) {
        nxv[i] = 0.0f;
    }
}

DSDFilters::~DSDFilters()
{
}

short DSDFilters::dmr_filter(short sample) // all 4800 baud filters for now
{
    return dsd_input_filter(sample, 3);
}

short DSDFilters::nxdn_filter(short sample) // all 2400 baud filters for now
{
    return dsd_input_filter(sample, 4);
}

short DSDFilters::dsd_input_filter(short sample, int mode)
{
    float sum;
    int i;
    float gain;
    int zeros;
    float *v;
    const float *coeffs;

    switch (mode)
    {
    case 1:
        gain = ngain;
        v = xv;
        coeffs = xcoeffs;
        zeros = NZEROS;
        break;
    case 2:
        gain = nxgain;
        v = nxv;
        coeffs = nxcoeffs;
        zeros = NXZEROS;
        break;
    case 3:
        gain = dmrgain;
        v = xv;
        coeffs = dmrcoeffs;
        zeros = NZEROS;
        break;
    case 4:
        gain = dpmrgain;
        v = nxv;
        coeffs = dpmrcoeffs;
        zeros = NXZEROS;
        break;
    default:
        return sample;
    }

    for (i = 0; i < zeros; i++) {
        v[i] = v[i + 1];
    }

    v[zeros] = sample; // unfiltered sample in
    sum = 0.0f;

    for (i = 0; i <= zeros; i++) {
        sum += (coeffs[i] * v[i]);
    }

    return (short) (sum / gain); // filtered sample out
}

// ====================================================================

DSDSecondOrderRecursiveFilter::DSDSecondOrderRecursiveFilter(float samplingFrequency, float centerFrequency, float r) :
		m_r(r),
		m_frequencyRatio(centerFrequency/samplingFrequency)
{
	init();
}

DSDSecondOrderRecursiveFilter::~DSDSecondOrderRecursiveFilter()
{}

void DSDSecondOrderRecursiveFilter::setFrequencies(float samplingFrequency, float centerFrequency)
{
	m_frequencyRatio = centerFrequency / samplingFrequency;
	init();
}

void DSDSecondOrderRecursiveFilter::setR(float r)
{
	m_r = r;
	init();
}

short DSDSecondOrderRecursiveFilter::run(short sample)
{
	m_v[0] = ((1.0f - m_r) * (float) sample) + (2.0f * m_r * cos(2.0*M_PI*m_frequencyRatio) * m_v[1]) - (m_r * m_r * m_v[2]);
	float y = m_v[0] - m_v[2];
	m_v[2] = m_v[1];
	m_v[1] = m_v[0];

	return (short) y;
}

void DSDSecondOrderRecursiveFilter::init()
{
	for (int i = 0; i < 3; i++)
	{
		m_v[i] = 0.0f;
	}
}

// ====================================================================

const float DSDMBEAudioInterpolatorFilter::m_lpa[3] = {1.0,           1.392667E+00, -5.474446E-01};
const float DSDMBEAudioInterpolatorFilter::m_lpb[3] = {3.869430E-02,  7.738860E-02,  3.869430E-02};
// f(-3dB) = 300 Hz @ 8000 Hz SR (w = 0.075):
const float DSDMBEAudioInterpolatorFilter::m_hpa[3] = {1.000000e+00,  1.667871e+00, -7.156964e-01};
const float DSDMBEAudioInterpolatorFilter::m_hpb[3] = {8.459039e-01, -1.691760e+00,  8.459039e-01};

DSDMBEAudioInterpolatorFilter::DSDMBEAudioInterpolatorFilter() :
        m_filterLP(m_lpa, m_lpb),
        m_filterHP(m_hpa, m_hpb),
        m_useHP(false)
{
}

DSDMBEAudioInterpolatorFilter::~DSDMBEAudioInterpolatorFilter()
{}

float DSDMBEAudioInterpolatorFilter::run(const float& sample)
{
    return m_useHP ? m_filterLP.run(m_filterHP.run(sample)) : m_filterLP.run(sample);
}

float DSDMBEAudioInterpolatorFilter::runHP(const float& sample)
{
    return m_filterHP.run(sample);
}

float DSDMBEAudioInterpolatorFilter::runLP(const float& sample)
{
    return m_filterLP.run(sample);
}

} // namespace dsdcc
