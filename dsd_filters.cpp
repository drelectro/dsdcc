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
//#include "pch.h"

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
{0.0301506278f, 0.0269200615f, 0.0159662432f, -0.0013114705f, -0.0216605133f, -0.0404938748f, -0.0528141756f, 
-0.0543747957f, -0.0428325003f, -0.0186176083f, 0.0147202645f, 0.0508418571f, 0.0816392577f, 0.0988113688f, 
0.0957187780f, 0.0691512084f, 0.0206194642f, -0.0431564563f, -0.1107569268f, -0.1675773224f, -0.1981519842f, 
-0.1889130786f, -0.1308939560f, -0.0218608492f, 0.1325685970f, 0.3190962499f, 0.5182530574f, 0.7070497652f, 
0.8623526878f, 0.9644213921f, 1.0000000000f, 0.9644213921f, 0.8623526878f, 0.7070497652f, 0.5182530574f, 
0.3190962499f, 0.1325685970f, -0.0218608492f, -0.1308939560f, -0.1889130786f, -0.1981519842f, -0.1675773224f, 
-0.1107569268f, -0.0431564563f, 0.0206194642f, 0.0691512084f, 0.0957187780f, 0.0988113688f, 0.0816392577f, 0.0508418571f,
0.0147202645f, -0.0186176083f, -0.0428325003f, -0.0543747957f, -0.0528141756f, -0.0404938748f, -0.0216605133f, 
-0.0013114705f, 0.0159662432f, 0.0269200615f, 0.0301506278f};

// dPMR filter - root raised cosine alpha=0.2 Ts = 3325 S/s Fc = 48 kHz - zero at boundaries appears to be slightly better
const float DSDFilters::dpmrcoeffs[] =
{-0.0000983004f, 0.0058388841f, 0.0119748846f, 0.0179185547f, 0.0232592816f,
        0.0275919612f, 0.0305433586f, 0.0317982965f, 0.0311240307f,
        0.0283911865f, 0.0235897433f, 0.0168387650f, 0.0083888763f,
        -0.0013831396f, -0.0119878087f, -0.0228442151f, -0.0333082708f,
        -0.0427067804f, -0.0503756642f, -0.0557003599f, -0.0581561791f,
        -0.0573462646f, -0.0530347941f, -0.0451732069f, -0.0339174991f,
        -0.0196350217f, -0.0028997157f, 0.0155246961f, 0.0347134030f,
        0.0536202583f, 0.0711271166f, 0.0861006725f, 0.0974542022f,
        0.1042112035f, 0.1055676660f, 0.1009496091f, 0.0900625944f,
        0.0729301774f, 0.0499186839f, 0.0217462748f, -0.0105250265f,
        -0.0455148664f, -0.0815673067f, -0.1168095612f, -0.1492246435f,
        -0.1767350726f, -0.1972941202f, -0.2089805758f, -0.2100926829f,
        -0.1992367833f, -0.1754063031f, -0.1380470370f, -0.0871052089f,
        -0.0230554989f, 0.0530929052f, 0.1398131936f, 0.2351006721f,
        0.3365341927f, 0.4413570929f, 0.5465745033f, 0.6490630781f,
        0.7456885564f, 0.8334261381f, 0.9094784589f, 0.9713859928f,
        1.0171250045f, 1.0451886943f, 1.0546479089f, 1.0451886943f,
        1.0171250045f, 0.9713859928f, 0.9094784589f, 0.8334261381f,
        0.7456885564f, 0.6490630781f, 0.5465745033f, 0.4413570929f,
        0.3365341927f, 0.2351006721f, 0.1398131936f, 0.0530929052f,
        -0.0230554989f, -0.0871052089f, -0.1380470370f, -0.1754063031f,
        -0.1992367833f, -0.2100926829f, -0.2089805758f, -0.1972941202f,
        -0.1767350726f, -0.1492246435f, -0.1168095612f, -0.0815673067f,
        -0.0455148664f, -0.0105250265f, 0.0217462748f, 0.0499186839f,
        0.0729301774f, 0.0900625944f, 0.1009496091f, 0.1055676660f,
        0.1042112035f, 0.0974542022f, 0.0861006725f, 0.0711271166f,
        0.0536202583f, 0.0347134030f, 0.0155246961f, -0.0028997157f,
        -0.0196350217f, -0.0339174991f, -0.0451732069f, -0.0530347941f,
        -0.0573462646f, -0.0581561791f, -0.0557003599f, -0.0503756642f,
        -0.0427067804f, -0.0333082708f, -0.0228442151f, -0.0119878087f,
        -0.0013831396f, 0.0083888763f, 0.0168387650f, 0.0235897433f,
        0.0283911865f, 0.0311240307f, 0.0317982965f, 0.0305433586f,
        0.0275919612f, 0.0232592816f, 0.0179185547f, 0.0119748846f,
        0.0058388841f, -0.0000983004f};

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
	m_v[0] = ((1.0f - m_r) * (float) sample) + (2.0f * m_r * (float)cos(2.0*M_PI*m_frequencyRatio) * m_v[1]) - (m_r * m_r * m_v[2]);
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

const float DSDMBEAudioInterpolatorFilter::m_lpa[3] = {1.0f,           1.392667E+00f, -5.474446E-01f};
const float DSDMBEAudioInterpolatorFilter::m_lpb[3] = {3.869430E-02f,  7.738860E-02f,  3.869430E-02f};
// f(-3dB) = 300 Hz @ 8000 Hz SR (w = 0.075):
const float DSDMBEAudioInterpolatorFilter::m_hpa[3] = {1.000000e+00f,  1.667871e+00f, -7.156964e-01f};
const float DSDMBEAudioInterpolatorFilter::m_hpb[3] = {8.459039e-01f, -1.691760e+00f,  8.459039e-01f};

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
