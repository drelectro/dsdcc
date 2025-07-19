///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Mike Cornelius, VK2XMC.                                    //       
// Based on the work of Edouard Griffiths, F4EXB.                                //
//                                                                               //                       
// Portions adapted from wireshark/plugins/p25/packet-p25cai.c                   //
// Copyright 2008, Michael Ossmann <mike@ossmann.com>                            //
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

#include "p25p1.h"
#include "dsd_decoder.h"
#include <vector>

// Suppress sprintf warnings
#pragma warning(disable: 4996)

namespace DSDcc
{

// Complete IMBE voice frame to dibit mapping
const int DSDP25P1::m_imbeMap[18][11] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
    {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21},
    {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
    {33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43},
    {44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54},
    {55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65},
    {66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76},
    {77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87},
    {88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98},
    {99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109},
    {110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120},
    {121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131},
    {132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142},
    {143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153},
    {154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164},
    {165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175},
    {176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186},
    {187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197}
};

// Deinterleave table for data and TSBK frames, ref BAAA 7.2
const unsigned int DSDcc::DSDP25P1::_dataPktMap[98] = {
    0, 1, 8,   9, 16, 17, 24, 25, 32, 33, 40, 41, 48, 49, 56, 57, 64, 65, 72, 73, 80, 81, 88, 89, 96, 97,
    2, 3, 10, 11, 18, 19, 26, 27, 34, 35, 42, 43, 50, 51, 58, 59, 66, 67, 74, 75, 82, 83, 90, 91,
    4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45, 52, 53, 60, 61, 68, 69, 76, 77, 84, 85, 92, 93,
    6, 7, 14, 15, 22, 23, 30, 31, 38, 39, 46, 47, 54, 55, 62, 63, 70, 71, 78, 79, 86, 87, 94, 95
};

DSDP25P1::DSDP25P1(DSDDecoder *dsdDecoder) :
    m_dsdDecoder(dsdDecoder),
    m_frameType(P25P1FrameNone),
    m_state(P25P1StateNone),
    m_symbolIndex(0),
    m_frameIndex(0),
    m_encrypted(false),
    m_emergency(false),
    m_nac(0),
    m_duid(0),
    m_talkGroup(0),
    m_source(0),
    m_algId(0),
    m_keyId(0),
    m_mfId(0),
    m_imbeFrameIndex(0),
    m_analogSignalIndex(0),
    m_viterbi_3_4(3, 4, Viterbi::Poly25),       // 3/4 rate for voice
    m_viterbi_1_2(1, 2, Viterbi::Poly25),       // 1/2 rate for TSBK/PDU 
    m_crcP25(CRC::PolyCCITT16, 16, 0x0000, 0xffff, 1, 0, 0)  
{
    memset(m_nidData, 0, sizeof(m_nidData));
    memset(m_lcData, 0, sizeof(m_lcData));
    memset(m_esData, 0, sizeof(m_esData));
    memset(m_rsData, 0, sizeof(m_rsData));  // Fix: was m.rsData
    memset(m_statusData, 0, sizeof(m_statusData));
    memset(m_imbeFrame, 0, sizeof(m_imbeFrame));
    memset(m_analogSignalArray, 0, sizeof(m_analogSignalArray));
}

DSDP25P1::~DSDP25P1()
{
}

void DSDP25P1::init()
{
    m_symbolIndex = 0;
	_statusIndex = 24; // By the time we get here we've already processed 24 dibits for the SYNC word
    m_frameIndex = 0;
    m_state = P25P1StateStartFrame;
    m_frameType = P25P1FrameNone;
    m_imbeFrameIndex = 0;
    m_analogSignalIndex = 0;
    
    // Clear all frame data buffers at start of each frame
    memset(m_nidData, 0, sizeof(m_nidData));
    memset(m_lcData, 0, sizeof(m_lcData));
    memset(m_esData, 0, sizeof(m_esData));
    memset(m_statusData, 0, sizeof(m_statusData));

	// The first thing we expect is a Network Identifier (NID) 
    _symbolsExpected = 8 * 4; // 8 Octets for NID

    // Reset P25 state
    m_encrypted = false;
    m_emergency = false;
    m_nac = 0;
    m_duid = 0;
    m_talkGroup = 0;
    m_source = 0;
    m_algId = 0;
    m_keyId = 0;
    
    m_dsdDecoder->m_voice1On = false;
}

void DSDP25P1::process()
{
    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit();
    
    // Store analog signal for heuristics
    storeAnalogSignal(m_dsdDecoder->m_dsdSymbol.getSymbolSyncSample(), dibit);

	// Every 35 dibits is a status symbol, these don't make it into the frame data buffer
    if ((_statusIndex+1) % 36 == 0 && _statusIndex > 0)
    {
        _statusIndex++;
		return; // Skip status symbols for now
	}
    _statusIndex++;

	// NID is not interleaved, so we can just place the dibit directly into the frame data buffer
    if (m_state == P25P1StateStartFrame)
    {
        // Acquire dibits and place into buffer
        int byteIndex = m_symbolIndex / 4;               // 4 dibits per byte
        int bitPosition = (3 - (m_symbolIndex % 4)) * 2; // bit position within byte (6,4,2,0)
        if (bitPosition == 6) _frameData[byteIndex] = 0; // Clear byte at start of new dibit
        _frameData[byteIndex] |= dibit << bitPosition;   // Set dibit in byte
    }
    else if (m_frameType = P25P1FrameTSBK)
    {
		_deinterleavedDibits[_dataPktMap[m_symbolIndex]] = dibit;
	}

    m_symbolIndex++;
	if (m_symbolIndex < _symbolsExpected)
    {
		return; // Not enough data yet
    }
    
    
    switch (m_state)
    {
    case P25P1StateStartFrame:
        processNID();
        m_symbolIndex = 0;
        m_state = P25P1StateFramePayload;
        break;
        
    case P25P1StateFramePayload:
        processFramePayload();
        break;
        
    default:
        break;
    }
}

void DSDP25P1::processHDU()
{
    // Header Data Unit processing
    init();
    m_frameType = P25P1FrameHDU;
    m_state = P25P1StateHDU;
}

void DSDP25P1::processNID()
{
    // Decode Network Identifier using BCH(63,16) shortened to (8,4)
    if (decodeBCH_63_16_5(_frameData))
    {
        m_nac = (_frameData[0] << 4) | (_frameData[1] >> 4);
        m_duid = _frameData[1] & 0x0F;
        
        //TRACE("P25: NAC %d DUID %0X\n", m_nac, m_duid);

        // Determine frame type from DUID
        switch (m_duid)
        {
        case 0x0:
            m_frameType = P25P1FrameHDU;
            TRACE("P25: Header Data Unit (HDU)\n");
            break;
        case 0x3:
            m_frameType = P25P1FrameTDULC;
            TRACE("P25: Terminator Data Unit with Link Control (TDULC)\n");
            break;
        case 0x5:
            m_frameType = P25P1FrameLDU1;
            m_dsdDecoder->m_voice1On = true;
            TRACE("P25: Logical Data Unit 1 (LDU1) - Voice\n");
            break;
        case 0x7:
            m_frameType = P25P1FrameTSBK;
            _symbolsExpected = 98; // 196 bits
            //TRACE("P25: Trunking System Block (TSBK)\n");
            break;
        case 0x9:
            m_frameType = P25P1FramePDU;
            TRACE("P25: Packet Data Unit (PDU)\n");
            break;
        case 0xA:
            m_frameType = P25P1FrameLDU2;
            m_dsdDecoder->m_voice1On = true;
            TRACE("P25: Logical Data Unit 2 (LDU2) - Voice\n");
            break;
        case 0xF:
            m_frameType = P25P1FrameTDU;
            m_dsdDecoder->m_voice1On = false;
            //TRACE("P25: Terminator Data Unit (TDU)\n");
            break;
        default:
            m_frameType = P25P1FrameNone;
            TRACE("P25: Unknown DUID: 0x%X NAC:%d\n", m_duid, m_nac);
            break;
        }
        
        // Update decoder state 
        m_dsdDecoder->m_state.nac = m_nac;
        sprintf_s(m_dsdDecoder->m_state.fsubtype, sizeof(m_dsdDecoder->m_state.fsubtype), " P25 DUID:%X  ", m_duid);
    }
    else
    {
        TRACE("P25: NID decode failed\n");
        m_frameType = P25P1FrameNone;
    }
}

void DSDP25P1::processFramePayload()
{
    switch (m_frameType)
    {
    case P25P1FrameLDU1:
        processLDU1();
        break;
    case P25P1FrameLDU2:
        processLDU2();
        break;
    case P25P1FrameTDU:
        processTDU();
        break;
    case P25P1FrameTDULC:
        processTDULC();
        break;
    case P25P1FrameTSBK:
        processTSBK();
        break;
    case P25P1FramePDU:
        processPDU();
        break;
    case P25P1FrameHDU:
        // HDU has no payload after NID
        m_dsdDecoder->resetFrameSync();
        break;
    default:
        m_dsdDecoder->resetFrameSync();
        break;
    }
}

void DSDP25P1::processLDU1()
{
    // LDU1: 1728 dibits total (including sync + NID)
    // Voice frames: 18 x 88 dibits = 1584 dibits
    // Link Control: 72 dibits
    // Status symbols: 72 dibits
    
    if (m_symbolIndex < 1584) // Voice data
    {
        int frameIdx = m_symbolIndex / 88;
        int symbolInFrame = m_symbolIndex % 88;
        
        if (frameIdx < 18)
        {
            processVoiceFrame(frameIdx);
        }
        
        m_symbolIndex++;
    }
    else if (m_symbolIndex < 1656) // Link Control (72 dibits)
    {
        if (m_symbolIndex == 1584)
        {
            memset(m_lcData, 0, sizeof(m_lcData));
        }

        int lcIndex = m_symbolIndex - 1584;
        m_lcData[lcIndex / 6] |= m_dsdDecoder->m_dsdSymbol.getDibit() << (4 - (lcIndex % 6) * 2);
        m_symbolIndex++;
        
        if (m_symbolIndex == 1656)
        {
            extractLinkControl();
        }
    }
    else if (m_symbolIndex < 1728) // Status symbols (72 dibits)
    {
        if (m_symbolIndex == 1656)
        {
            memset(m_statusData, 0, sizeof(m_statusData));
        }

        int statusIndex = m_symbolIndex - 1656;
        m_statusData[statusIndex / 4] |= m_dsdDecoder->m_dsdSymbol.getDibit() << (6 - (statusIndex % 4) * 2);
        m_symbolIndex++;
        
        if (m_symbolIndex == 1728)
        {
            extractStatusSymbols();
            processHeuristics();
            m_dsdDecoder->resetFrameSync();
        }
    }
}

void DSDP25P1::processLDU2()
{
    // Similar to LDU1 but with Encryption Sync instead of Link Control
    if (m_symbolIndex < 1584) // Voice data
    {
        int frameIdx = m_symbolIndex / 88;
        processVoiceFrame(frameIdx);
        m_symbolIndex++;
    }
    else if (m_symbolIndex < 1656) // Encryption Sync (72 dibits)
    {
        if (m_symbolIndex == 1584)
        {
            memset(m_esData, 0, sizeof(m_esData));
        }

        int esIndex = m_symbolIndex - 1584;
        m_esData[esIndex / 18] |= m_dsdDecoder->m_dsdSymbol.getDibit() << (16 - (esIndex % 18) * 2);
        m_symbolIndex++;
        
        if (m_symbolIndex == 1656)
        {
            extractEncryptionSync();
        }
    }
    else if (m_symbolIndex < 1728) // Status symbols
    {
        int statusIndex = m_symbolIndex - 1656;
        m_statusData[statusIndex / 4] |= m_dsdDecoder->m_dsdSymbol.getDibit() << (6 - (statusIndex % 4) * 2);
        m_symbolIndex++;
        
        if (m_symbolIndex == 1728)
        {
            extractStatusSymbols();
            processHeuristics();
            m_dsdDecoder->resetFrameSync();
        }
    }
}

void DSDP25P1::processVoiceFrame(int frameIndex)
{
    if (frameIndex >= 18) return;
    
    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit();
    int symbolInFrame = m_symbolIndex % 88;
    
    // Extract IMBE voice data (simplified mapping)
    if (symbolInFrame < 88) // Each voice frame has 88 dibits
    {
        extractIMBE(m_imbeFrame[frameIndex], frameIndex);
        
        // Process complete IMBE frame
        if (symbolInFrame == 87)
        {
            if (m_dsdDecoder->m_mbelibEnable)
            {
                // Fix: Use ambe_fr instead of imbe_fr to match processFrame signature
                memset(m_dsdDecoder->ambe_fr[frameIndex % 4], 0, 24);
                
                // Convert IMBE frame to the format expected by mbelib
                for (int i = 0; i < 88 && i < 24 * 8; i++) // Prevent buffer overflow
                {
                    int byteIndex = i / 8;
                    int bitIndex = i % 8;
                    if (byteIndex < 11 && byteIndex < 24 && m_imbeFrame[frameIndex][byteIndex] & (1 << (7 - bitIndex)))
                    {
                        m_dsdDecoder->ambe_fr[frameIndex % 4][i / 8] |= (1 << (7 - (i % 8)));
                    }
                }
                
                // Process with MBE decoder when we have enough frames
                if ((frameIndex % 4) == 3)
                {
                    // Fix: Use correct signature for processFrame
                    m_dsdDecoder->m_mbeDecoder1.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                }
            }
            
            // Store for DVSI - Fix: Check bounds properly
            if (frameIndex < 18 && frameIndex < (int)(sizeof(m_dsdDecoder->m_mbeDVFrame1)))
            {
                size_t copySize = std::min(sizeof(m_dsdDecoder->m_mbeDVFrame1) - frameIndex, (size_t)11);
                memcpy(&m_dsdDecoder->m_mbeDVFrame1[frameIndex], m_imbeFrame[frameIndex], copySize);
                m_dsdDecoder->m_mbeDVReady1 = true;
            }
        }
    }
}

void DSDP25P1::extractIMBE(unsigned char* imbeFrame, int frameIndex)
{
    // Simplified IMBE extraction - would need full P25 voice frame structure
    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit();
    int symbolInFrame = m_symbolIndex % 88;
    
    if (symbolInFrame < 44) // First half of voice frame
    {
        imbeFrame[symbolInFrame / 4] |= dibit << (6 - (symbolInFrame % 4) * 2);
    }
}

void DSDP25P1::extractLinkControl()
{
    // Decode Link Control with Golay(24,12,8) error correction
    if (decodeGolay_24_12_8(m_lcData))
    {
        // Extract talk group and source from link control
        m_talkGroup = (m_lcData[2] << 8) | m_lcData[3];
        m_source = (m_lcData[4] << 16) | (m_lcData[5] << 8) | m_lcData[6];
        
        // Check for emergency bit
        m_emergency = (m_lcData[0] & 0x80) != 0;
        
        // Update decoder state
        m_dsdDecoder->m_state.lasttg = m_talkGroup;
        m_dsdDecoder->m_state.lastsrc = m_source;
        
        TRACE("P25: TG:%d SRC:%d %s\n", m_talkGroup, m_source, m_emergency ? "EMERGENCY" : "");
    }
}

void DSDP25P1::extractEncryptionSync()
{
    // Decode Encryption Sync
    m_algId = m_esData[0];
    m_keyId = (m_esData[1] << 8) | m_esData[2];
    m_encrypted = (m_algId != 0x80); // 0x80 = unencrypted
    
    // Update decoder state - Fix: Use sprintf_s for safety
    sprintf_s(m_dsdDecoder->m_state.algid, sizeof(m_dsdDecoder->m_state.algid), "%02X", m_algId);
    sprintf_s(m_dsdDecoder->m_state.keyid, sizeof(m_dsdDecoder->m_state.keyid), "%04X", m_keyId);
    
    TRACE("P25: ALG:%02X KEY:%04X %s\n", m_algId, m_keyId, m_encrypted ? "ENCRYPTED" : "CLEAR");
}

void DSDP25P1::extractStatusSymbols()
{
    // Process status symbols if needed
    TRACE("P25: Status symbols processed\n");
}

void DSDP25P1::processTDU()
{
    // Terminator Data Unit - end voice transmission
    m_dsdDecoder->m_voice1On = false;
    //TRACE("P25: TDU - End transmission\n");
    m_dsdDecoder->resetFrameSync();
}

void DSDP25P1::processTDULC()
{
    // Terminator with Link Control
    processTDU();
}

void DSDP25P1::processTSBK()
{
    // Trunking System Block - control signaling
    // Ref: TIA-102.AABB-B and TIA-102.AABC-B

    // TSBK is encoded with 1/2 rate Trellis coding
    if (decodeTrellis_1_2() == false)
    {
        TRACE("P25: TSBK decode error\n");
        m_dsdDecoder->resetFrameSync();
		return; // Decode error, return failure
    }
	// Now we have the decoded TSBK data in _frameData
 
    // Check CRC-16 on decoded data (first 10 bytes data + 2 bytes CRC)
    unsigned short receivedCRC = (_frameData[10] << 8) | _frameData[11];
    unsigned short calculatedCRC = (unsigned short)m_crcP25.crcbitbybit(_frameData, 10);

    // Extract TSBK fields from decoded data
    TSBK tsbk;
    tsbk.opcode = _frameData[0];
    tsbk.mfId = _frameData[1];
    memcpy(tsbk.args, &_frameData[2], 8);
    tsbk.crc = receivedCRC;

    unsigned char lb = (tsbk.opcode >> 7) & 0x01; // Last Block Flag
    unsigned char pf = (tsbk.opcode >> 6) & 0x01; // Protected Flag
    tsbk.opcode &= 0x3F; // Clear flags, keep opcode

    TRACE("P25: TSBK(%c%c) Opcode: 0x%02X MFID: 0x%02X CRC: %s\n",
        lb ? 'L' : '-', pf ? 'P' : '-', tsbk.opcode, tsbk.mfId,
        (receivedCRC == calculatedCRC) ? "OK" : "FAIL");

    if (tsbk.mfId == 0x00 || tsbk.mfId == 0x01)
    {
        processTSBKOpcode(tsbk);
    }
    else
    {
        TRACE("P25: TSBK Manufacturer ID not recognized: 0x%02X\n", tsbk.mfId);
	}

    // Reset for next frame
    m_dsdDecoder->resetFrameSync();
}
void DSDP25P1::processTSBKOpcode(TSBK& tsbk)
{
    // Handle specific TSBK opcodes here
    switch (tsbk.opcode)
    {
    case 0x01: // Example opcode for system information
		TRACE("P25: TSBK System Information MFID: 0x%02X\n", tsbk.mfId);
        break;
    case 0x02: // Example opcode for emergency
		TRACE("P25: TSBK Emergency MFID: 0x%02X\n", tsbk.mfId);
        break;

    case 0x3B: // Network Status Broadcast
		processNetworkStatusBroadcast(tsbk);
        break;

    case 0x3C: // Example opcode for channel grant
		// Process channel grant
       
		break;

    default:
        TRACE("P25: Unknown TSBK opcode: 0x%02X\n", tsbk.opcode);
        break;
    }
}

void DSDP25P1::processNetworkStatusBroadcast(TSBK& tsbk)
{
    int LRA = tsbk.args[0]; // Logical Radio Address
    int WACNID = (tsbk.args[1] << 12) | (tsbk.args[2] << 4) | (tsbk.args[3] >> 4); // WACN ID
    int SystemID = ((tsbk.args[3] & 0x0F) << 8) | tsbk.args[4]; // System ID
    int Channel = ((tsbk.args[5] & 0xF0) >> 4) | (tsbk.args[6] & 0x0F); // Channel Number
    int ServiceClass = tsbk.args[7]; // Service Class

    //TRACE("P25: TSBK Network Status Broadcast: LRA:%d WACNID:%04X SystemID:%04X Channel:%d ServiceClass:%d\n",
    //    LRA, WACNID, SystemID, Channel, ServiceClass);

    std::cerr << "P25: TSBK Network Status Broadcast: LRA:" << LRA
              << " WACNID:" << std::hex << WACNID
              << " SystemID:" << SystemID
              << " Channel:" << Channel
		<< " ServiceClass:" << ServiceClass << std::dec << std::endl;
}

void DSDP25P1::processPDU()
{
    // Packet Data Unit - data transmission
    TRACE("P25: PDU processing not implemented\n");
    m_dsdDecoder->resetFrameSync();
}

int DSDP25P1::find_min(uint8_t list[], int len)
{
    int min = list[0];
    int index = 0;
    int unique = 1;
    int i;

    for (i = 1; i < len; i++) {
        if (list[i] < min) {
            min = list[i];
            index = i;
            unique = 1;
        }
        else if (list[i] == min) {
            unique = 0;
        }
    }
    /* return -1 if a minimum can't be found */
    if (!unique)
        return -1;

    return index;
}

int DSDP25P1::count_bits(unsigned int n)
{
    int i = 0;
    for (i = 0; n != 0; i++)
        n &= n - 1;
    return i;
}

bool DSDP25P1::decodeHamming_10_6_3(unsigned char* data)
{
    return m_hamming_10_6_3.decode(data);
}

bool DSDP25P1::decodeGolay_24_12_8(unsigned char* data)
{
    return m_golay_24_12_8.decode(data);
}

bool DSDP25P1::decodeReedSolomon_24_12_13(unsigned char* data, int blocks)
{
    // Reed-Solomon decoding would be implemented here
    return true; // Assume success for now
}

bool DSDP25P1::decodeBCH_63_16_5(unsigned char* data)
{
    return m_bch_63_16_5.decode(data);
}

bool DSDP25P1::decodeTrellis_3_4(unsigned char* data, int length)
{
    // Fix: Use std::vector instead of VLA for C++ compatibility
    std::vector<unsigned char> decoded(length * 3 / 4);
    m_viterbi_3_4.decodeFromBits(decoded.data(), data, length, 0);
    memcpy(data, decoded.data(), length * 3 / 4);
    return true;
}

// Decode deinterleaved dibits using 1/2 rate Trellis coding
// This is used for TSBK and PDU frames and is always 98 dibits input 48 dibits output per BAAA section 7
bool DSDP25P1::decodeTrellis_1_2()
{
    int i, j;
    int state = 0;
    uint8_t codeword;
    uint8_t hd[4];  // Hamming distances for each candidate codeword

    static const uint8_t next_words[4][4] = {
        {0x2, 0xC, 0x1, 0xF},
        {0xE, 0x0, 0xD, 0x3},
        {0x9, 0x7, 0xA, 0x4},
        {0x5, 0xB, 0x6, 0x8}
    };

    memset(_frameData, 0, sizeof(_frameData));

    /* step through 4 bit codewords in input */
    for (i = 0; i < 98; i += 2) {
        codeword = ((_deinterleavedDibits[i] << 2) | (_deinterleavedDibits[i + 1]));
        /* try each codeword in a row of the state transition table */
        for (j = 0; j < 4; j++) {
            /* find Hamming distance for candidate */
            hd[j] = count_bits(codeword ^ next_words[state][j]);
        }
        /* find the dibit that matches the most codeword bits (minimum Hamming distance) */
        state = find_min(hd, 4);
        /* error if minimum can't be found */
        if (state == -1)
        {
            //TRACE("P25: TSBK decode error at dibit %d, codeword: %02X\n", i / 2, codeword);
            m_dsdDecoder->resetFrameSync();
            return false;	// decode error, return failure
        }
        /* It also might be nice to report a condition where the minimum is
         * non-zero, i.e. an error has been corrected.  It probably shouldn't
         * be a permanent failure, though.
         *
         * DISSECTOR_ASSERT(hd[state] == 0);
         */

         /* append dibit onto output buffer */
        if (i < 96)
        {
            int bitPosition = (6 - (((i / 2) % 4) * 2));
            _frameData[(i / 8)] |= state << bitPosition;
        }
    }
    return true;
}

void DSDP25P1::storeAnalogSignal(int value, int dibit)
{
    if (m_analogSignalIndex < 400)
    {
        m_analogSignalArray[m_analogSignalIndex].value = value;
        m_analogSignalArray[m_analogSignalIndex].dibit = dibit;
        m_analogSignalArray[m_analogSignalIndex].corrected_dibit = dibit;
        m_analogSignalArray[m_analogSignalIndex].sequence_broken = 0;
        m_analogSignalIndex++;
    }
}

void DSDP25P1::processHeuristics()
{
    // Contribute to P25 heuristics for improved symbol detection
    if (m_analogSignalIndex > 0)
    {
        DSDP25Heuristics::contribute_to_heuristics(
            0, // C4FM modulation
            &m_dsdDecoder->m_state.p25_heuristics,
            m_analogSignalArray,
            m_analogSignalIndex
        );
        m_analogSignalIndex = 0;
    }
}

} // namespace DSDcc