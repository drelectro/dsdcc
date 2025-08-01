/*
 *   Copyright (C) 2009-2016,2018 by Jonathan Naylor G4KLX
 *
 *   Copyright (C) 2018 by Edouard Griffiths F4EXB:
 *   - Cosmetic changes to integrate with DSDcc
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
//#include "pch.h"

#include "nxdnconvolution.h"

#include <cstdlib>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

namespace DSDcc
{

const uint8_t CNXDNConvolution::BIT_MASK_TABLE[] = {0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U};

#define WRITE_BIT1(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])
#define READ_BIT1(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])

const uint8_t CNXDNConvolution::BRANCH_TABLE1[] = {0U, 0U, 0U, 0U, 2U, 2U, 2U, 2U};
const uint8_t CNXDNConvolution::BRANCH_TABLE2[] = {0U, 2U, 2U, 0U, 0U, 2U, 2U, 0U};

const unsigned int CNXDNConvolution::NUM_OF_STATES_D2 = 8U;
const unsigned int CNXDNConvolution::NUM_OF_STATES = 16U;
const uint32_t     CNXDNConvolution::M = 4U;
const unsigned int CNXDNConvolution::K = 5U;

CNXDNConvolution::CNXDNConvolution() :
m_metrics1(0),
m_metrics2(0),
m_oldMetrics(0),
m_newMetrics(0),
m_decisions(0),
m_dp(0)
{
	m_metrics1  = new uint16_t[16U];
	m_metrics2  = new uint16_t[16U];
	m_decisions = new uint64_t[300U];
}

CNXDNConvolution::~CNXDNConvolution()
{
	delete[] m_metrics1;
	delete[] m_metrics2;
	delete[] m_decisions;
}

void CNXDNConvolution::start()
{
	::memset(m_metrics1, 0x00U, NUM_OF_STATES * sizeof(uint16_t));
	::memset(m_metrics2, 0x00U, NUM_OF_STATES * sizeof(uint16_t));

	m_oldMetrics = m_metrics1;
	m_newMetrics = m_metrics2;
	m_dp = m_decisions;
}

void CNXDNConvolution::decode(uint8_t s0, uint8_t s1)
{
  *m_dp = 0U;

  for (uint8_t i = 0U; i < NUM_OF_STATES_D2; i++) {
    uint8_t j = i * 2U;

    uint16_t metric = std::abs(BRANCH_TABLE1[i] - s0) + std::abs(BRANCH_TABLE2[i] - s1);

    uint16_t m0 = m_oldMetrics[i] + metric;
    uint16_t m1 = m_oldMetrics[i + NUM_OF_STATES_D2] + (M - metric);
    uint8_t decision0 = (m0 >= m1) ? 1U : 0U;
    m_newMetrics[j + 0U] = decision0 != 0U ? m1 : m0;

    m0 = m_oldMetrics[i] + (M - metric);
    m1 = m_oldMetrics[i + NUM_OF_STATES_D2] + metric;
    uint8_t decision1 = (m0 >= m1) ? 1U : 0U;
    m_newMetrics[j + 1U] = decision1 != 0U ? m1 : m0;

    *m_dp |= (uint64_t(decision1) << (j + 1U)) | (uint64_t(decision0) << (j + 0U));
  }

  ++m_dp;

  assert((m_dp - m_decisions) <= 300);

  uint16_t* tmp = m_oldMetrics;
  m_oldMetrics = m_newMetrics;
  m_newMetrics = tmp;
}

void CNXDNConvolution::chainback(unsigned char* out, unsigned int nBits)
{
	assert(out != 0);

	uint32_t state = 0U;

	while (nBits-- > 0) {
		--m_dp;

		uint32_t  i = state >> (9 - K);
		uint8_t bit = uint8_t(*m_dp >> i) & 1;
		state = (bit << 7) | (state >> 1);

		WRITE_BIT1(out, nBits, bit != 0U);
	}
}

void CNXDNConvolution::encode(const unsigned char* in, unsigned char* out, unsigned int nBits) const
{
	assert(in != 0);
	assert(out != 0);
	assert(nBits > 0U);

	uint8_t d1 = 0U, d2 = 0U, d3 = 0U, d4 = 0U;
	uint32_t k = 0U;
	for (unsigned int i = 0U; i < nBits; i++) {
		uint8_t d = READ_BIT1(in, i) ? 1U : 0U;

		uint8_t g1 = (d + d3 + d4) & 1;
		uint8_t g2 = (d + d1 + d2 + d4) & 1;

		d4 = d3;
		d3 = d2;
		d2 = d1;
		d1 = d;

		WRITE_BIT1(out, k, g1 != 0U);
		k++;

		WRITE_BIT1(out, k, g2 != 0U);
		k++;
	}
}

} // namespace
