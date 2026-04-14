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
#include "dsd_logger.h"
#include "dsd_decoder.h"
#include <algorithm>
#include <vector>
#include <sstream>

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
    memset(_deinterleavedDibits, 0, sizeof(_deinterleavedDibits));

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
	else if (m_frameType == P25P1FrameTSBK)
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
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_networkState.nac = (uint16_t)m_nac;
        }
        
        //TRACE("P25: NAC %d DUID %0X\n", m_nac, m_duid);

        // Determine frame type from DUID
        switch (m_duid)
        {
        case 0x0:
            m_frameType = P25P1FrameHDU;
            //TRACE("P25: Header Data Unit (HDU)\n");
            break;
        case 0x3:
            m_frameType = P25P1FrameTDULC;
            //TRACE("P25: Terminator Data Unit with Link Control (TDULC)\n");
            break;
        case 0x5:
            m_frameType = P25P1FrameLDU1;
            m_dsdDecoder->m_voice1On = true;
            //TRACE("P25: Logical Data Unit 1 (LDU1) - Voice\n");
            break;
        case 0x7:
            m_frameType = P25P1FrameTSBK;
            _symbolsExpected = 98; // 196 bits
            //TRACE("P25: Trunking System Block (TSBK)\n");
            break;
        case 0xA:
            m_frameType = P25P1FrameLDU2;
            m_dsdDecoder->m_voice1On = true;
            //TRACE("P25: Logical Data Unit 2 (LDU2) - Voice\n");
            break;
        case 0xC:
            m_frameType = P25P1FramePDU;
            //TRACE("P25: Packet Data Unit (PDU)\n");
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
    else if (m_symbolIndex <= 1728) // Status symbols (72 dibits)
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
    else {
        m_dsdDecoder->resetFrameSync();
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
    else if (m_symbolIndex <= 1728) // Status symbols
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
    else {
		m_dsdDecoder->resetFrameSync();
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
                size_t copySize = (std::min)(sizeof(m_dsdDecoder->m_mbeDVFrame1) - frameIndex, (size_t)11);
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

    // TSBK is encoded with 1/2 rate Trellis coding; decodeTrellis_1_2 always
    // produces a best-effort output — CRC is the final validity gate.
    decodeTrellis_1_2();

	// Now we have the decoded TSBK data in _frameData
 
    // Check CRC-16 on decoded data (first 10 bytes data + 2 bytes CRC)
    unsigned short receivedCRC = (_frameData[10] << 8) | _frameData[11];
    unsigned short calculatedCRC = (unsigned short)m_crcP25.crcbitbybit(_frameData, 10);
    bool crcOk = (receivedCRC == calculatedCRC);

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_networkState.tsbkTotalCount++;
        if (crcOk) m_networkState.crcOkCount++;
        else        m_networkState.crcFailCount++;
    }

    if (!crcOk)
    {
        TRACE("P25: TSBK CRC fail\n");
        m_dsdDecoder->resetFrameSync();
        return;
    }

    // Extract TSBK fields from decoded data
    TSBK tsbk;
    tsbk.opcode = _frameData[0];
    tsbk.mfId = _frameData[1];
    memcpy(tsbk.args, &_frameData[2], 8);
    tsbk.crc = receivedCRC;

    unsigned char lb = (tsbk.opcode >> 7) & 0x01; // Last Block Flag
    unsigned char pf = (tsbk.opcode >> 6) & 0x01; // Protected Flag
    tsbk.opcode &= 0x3F; // Clear flags, keep opcode

    //TRACE("P25: TSBK(%c%c) Opcode: 0x%02X MFID: 0x%02X CRC: %s\n",
    //    lb ? 'L' : '-', pf ? 'P' : '-', tsbk.opcode, tsbk.mfId,
    //    crcOk ? "OK" : "FAIL");

    if (tsbk.mfId == 0x00 || tsbk.mfId == 0x01)
    {
        processTSBKOpcode(tsbk);
    }
    else if (tsbk.mfId == 0x90)
    {
        processTSBKOpcodeMoto(tsbk);
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
	// Handle specific TSBK opcodes here - Ref: TIA-102.AABC-B for opcode definitions
    switch (tsbk.opcode)
    {

	case 0x00: // GRP_V_CH_GRANT Group Voice Channel Grant (s5.1)
		processGroupVoiceChannelGrant(tsbk);
		break;

	case 0x02: // GRP_V_CH_GRANT_UPDT Group Voice Channel Grant Update (s5.1)
		processGroupVoiceChannelGrantUpdate(tsbk);
		break;

	case 0x16: // IND_DATA_REQ / SNDCP_CH_GRN Individual Data Service Request (s5.1)
		processIndividualDataRequest(tsbk);
		break;

	case 0x30: // TIME_DATE_ANNC Time and Date Announcement (s6.2)
		processTimeDateAnnouncement(tsbk);
		break;

    case 0x33: // IDEN_UP_TDMA Channel Identifier Update TDMA (s6.2)
		processIdenUpdateTDMA(tsbk);
		break;

    case 0x34: // IDEN_UP_VU IDEN Update Voice Unit ID (s6.2)
		processIdenUpdateVU(tsbk);
		break;

	case 0x39: // SCCB Secondary Control Channel Broadcast (s6.2)
		processSecondaryControlChannelBroadcast(tsbk);
		break;

	case 0x3A: //RFSS_STS_BCST RF Subsystem Status Broadcast (s6.2)
		processRFSSStatusBroadcast(tsbk);
		break;

	case 0x3B: // NET_STS_BCST Network Status Broadcast (s6.2)
		processNetworkStatusBroadcast(tsbk);
        break;

    case 0x3C: // ADJ_STS_BCST Adjacent Status Broadcast (s6.2)
		processAdjacentStatusBroadcast(tsbk);
		break;

	case 0x3D: // IDEN_UP Channel Identifier Update (s6.2)
		processIdenUpdate(tsbk);
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

    //DSD_LOG("P25: TSBK Network Status Broadcast: LRA:" << LRA
    //        << " WACNID:0x" << std::hex << WACNID
    //        << " SystemID:0x" << SystemID
    //        << " Channel:0x" << Channel
    //        << " ServiceClass:0x" << ServiceClass);

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_networkState.lra            = (uint8_t)LRA;
        m_networkState.wacnId         = (uint32_t)WACNID;
        m_networkState.netSystemId    = (uint16_t)SystemID;
        m_networkState.netChannel     = (uint16_t)Channel;
        m_networkState.netServiceClass = (uint8_t)ServiceClass;
    }
}

void DSDP25P1::processIndividualDataRequest(TSBK& tsbk)
{
    // TIA-102.AABC-B s5.1 / SNDCP_CH_GRN
    int ServiceOpts = tsbk.args[0];                            // Service options byte
    int Channel1    = (tsbk.args[2] << 8) | tsbk.args[3];     // Transmit channel (ID + number)
    int Channel2    = (tsbk.args[4] << 8) | tsbk.args[5];     // Receive channel (ID + number)

    DSD_LOG("P25: TSBK Individual Data Request: ServiceOpts:0x" << std::hex << ServiceOpts
            << " Ch1:0x" << Channel1
            << " Ch2:0x" << Channel2);
}

void DSDP25P1::processIdenUpdateVU(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 IDEN_UP_VU - Channel Identifier Update (VHF/UHF)
    int Identifier  = (tsbk.args[0] >> 4) & 0x0F;                                          // Channel identifier (4 bits)
    int BWtype      = tsbk.args[0] & 0x0F;                                                  // Bandwidth type (4 bits)
    int TXOffset0   = (tsbk.args[1] << 6) | ((tsbk.args[2] >> 2) & 0x3F);                  // TX offset raw (14 bits)
    int Spacing     = ((tsbk.args[2] & 0x03) << 8) | tsbk.args[3];                         // Channel spacing (10 bits, units 125 Hz)
    long BaseFreq   = ((long)tsbk.args[4] << 24) | ((long)tsbk.args[5] << 16)
                    | ((long)tsbk.args[6] << 8)  | tsbk.args[7];                            // Base frequency (32 bits, units 5 Hz)

    // Decode signed TX offset: bit 13 is sign (0 = negative offset, 1 = positive)
    int toff_sign  = (TXOffset0 >> 13) & 0x1;
    int toff_mag   = TXOffset0 & 0x1FFF;
    long TXOffsetHz = (long)toff_mag * Spacing * 125;
    if (toff_sign == 0)
        TXOffsetHz = -TXOffsetHz;

    //DSD_LOG("P25: TSBK IDEN Update VU: ID:" << Identifier
    //        << " BW:" << BWtype
    //        << " BaseFreq:" << (BaseFreq * 5) << "Hz"
    //        << " Spacing:" << (Spacing * 125) << "Hz"
    //        << " TXOffset:" << TXOffsetHz << "Hz");

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto& ci = m_networkState.channelIdents[Identifier & 0x0F];
        ci.valid          = true;
        ci.baseFreqHz     = BaseFreq * 5;
        ci.spacingHz      = (int64_t)Spacing * 125;
        ci.txOffsetHz     = TXOffsetHz;
        ci.bwHz           = 0;      // BWtype is a coded value; use spacingHz for channel width
        ci.slotsPerCarrier = 1;
        ci.isTDMA         = false;
    }
}

void DSDP25P1::processRFSSStatusBroadcast(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 RFSS_STS_BCST - RF Subsystem Status Broadcast
    int Flags       = tsbk.args[0];                                    // bit 7 = ROAM, bit 6 = ELK
    int SystemID    = ((tsbk.args[1] & 0x0F) << 8) | tsbk.args[2];    // System ID (12 bits)
    int RFSSID      = tsbk.args[3];                                    // RF Subsystem ID (8 bits)
    int SiteID      = tsbk.args[4];                                    // Site ID (8 bits)
    int Channel     = (tsbk.args[5] << 8) | tsbk.args[6];             // Channel (ID 4 bits + number 12 bits)
    int ServiceClass = tsbk.args[7];                                   // Service Class (8 bits)

    //DSD_LOG("P25: TSBK RFSS Status Broadcast: ROAM:" << ((Flags >> 7) & 1)
    //        << " ELK:" << ((Flags >> 6) & 1)
    //        << " SystemID:0x" << std::hex << SystemID
    //        << " RFSSID:" << std::dec << RFSSID
    //        << " SiteID:" << SiteID
    //        << " Channel:0x" << std::hex << Channel
    //        << " ServiceClass:0x" << ServiceClass);

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_networkState.rfssFlags       = (uint8_t)Flags;
        m_networkState.rfssSystemId    = (uint16_t)SystemID;
        m_networkState.rfssId          = (uint8_t)RFSSID;
        m_networkState.siteId          = (uint8_t)SiteID;
        m_networkState.rfssChannel     = (uint16_t)Channel;
        m_networkState.rfssServiceClass = (uint8_t)ServiceClass;
    }
}

void DSDP25P1::processAdjacentStatusBroadcast(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 ADJ_STS_BCST - Adjacent Site Status Broadcast
    int CFVA        = (tsbk.args[0] >> 4) & 0x07;                     // CFVA flags (3 bits: Conventional/Failure/Valid/Active)
    int SystemID    = ((tsbk.args[1] & 0x0F) << 8) | tsbk.args[2];    // Adjacent System ID (12 bits)
    int RFSSID      = tsbk.args[3];                                    // Adjacent RF Subsystem ID (8 bits)
    int SiteID      = tsbk.args[4];                                    // Adjacent Site ID (8 bits)
    int Channel     = (tsbk.args[5] << 8) | tsbk.args[6];             // Channel (ID 4 bits + number 12 bits)
    int ServiceClass = tsbk.args[7];                                   // Service Class (8 bits)

    //DSD_LOG("P25: TSBK Adjacent Status Broadcast: CFVA:0x" << std::hex << CFVA
    //        << " SystemID:0x" << SystemID
    //        << " RFSSID:" << std::dec << RFSSID
    //        << " SiteID:" << SiteID
    //        << " Channel:0x" << std::hex << Channel
    //        << " ServiceClass:0x" << ServiceClass);
}

void DSDP25P1::processGroupVoiceChannelGrant(TSBK& tsbk)
{
    // TIA-102.AABC-B s5.1 GRP_V_CH_GRANT - Group Voice Channel Grant
    int ServiceOpts = tsbk.args[0];                                                        // Service options (Emergency b7, Encrypted b6, Priority b2:0)
    int Channel     = (tsbk.args[1] << 8) | tsbk.args[2];                                 // Channel (Identifier 4 bits + Channel Number 12 bits)
    int GroupAddr   = (tsbk.args[3] << 8) | tsbk.args[4];                                 // Destination Group Address (TGID)
    int SourceAddr  = (tsbk.args[5] << 16) | (tsbk.args[6] << 8) | tsbk.args[7];         // Source Address

    // Record active channel in network state
    auto nowMs = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto& ac = m_networkState.activeChannels;
        auto it = std::find_if(ac.begin(), ac.end(),
            [Channel, GroupAddr](const P25NetworkState::ActiveChannel& e) {
                return e.channel == (uint16_t)Channel && e.tgid == (uint16_t)GroupAddr;
            });
        if (it != ac.end()) {
            it->srcAddr    = (uint32_t)SourceAddr;
            it->encrypted  = (ServiceOpts >> 6) & 1;
            it->lastSeenMs = nowMs;
        } else {
            P25NetworkState::ActiveChannel entry{};
            entry.channel   = (uint16_t)Channel;
            entry.tgid      = (uint16_t)GroupAddr;
            entry.srcAddr   = (uint32_t)SourceAddr;
            entry.encrypted = (ServiceOpts >> 6) & 1;
            entry.lastSeenMs = nowMs;
            ac.push_back(entry);
        }
        // Expire entries older than 10 s
        ac.erase(std::remove_if(ac.begin(), ac.end(),
            [nowMs](const P25NetworkState::ActiveChannel& e) {
                return (nowMs - e.lastSeenMs) > 10000;
            }), ac.end());

        // Update discovered talk groups (never expires)
        auto& dtg = m_networkState.discoveredTalkGroups;
        auto dtgIt = std::find_if(dtg.begin(), dtg.end(),
            [GroupAddr](const P25NetworkState::DiscoveredTalkGroup& e) {
                return e.tgid == (uint16_t)GroupAddr;
            });
        if (dtgIt != dtg.end()) {
            dtgIt->lastSeenMs = nowMs;
            dtgIt->encrypted  = (ServiceOpts >> 6) & 1;
            ++dtgIt->callCount;
        } else {
            P25NetworkState::DiscoveredTalkGroup tg{};
            tg.tgid        = (uint16_t)GroupAddr;
            tg.firstSeenMs = nowMs;
            tg.lastSeenMs  = nowMs;
            tg.callCount   = 1;
            tg.encrypted   = (ServiceOpts >> 6) & 1;
            dtg.push_back(tg);
        }
    }

    DSD_LOG("P25: TSBK Group Voice Channel Grant: ServiceOpts:0x" << std::hex << ServiceOpts
            << " Ch:0x" << Channel
            << " TGID:" << std::dec << GroupAddr
            << " SrcAddr:" << SourceAddr);
}

void DSDP25P1::processGroupVoiceChannelGrantUpdate(TSBK& tsbk)
{
    // TIA-102.AABC-B s5.1 GRP_V_CH_GRANT_UPDT - Group Voice Channel Grant Update
    int Channel1   = (tsbk.args[0] << 8) | tsbk.args[1];   // Channel 1 (Transmit)
    int GroupAddr1 = (tsbk.args[2] << 8) | tsbk.args[3];   // Group Address 1
    int Channel2   = (tsbk.args[4] << 8) | tsbk.args[5];   // Channel 2 (Receive)
    int GroupAddr2 = (tsbk.args[6] << 8) | tsbk.args[7];   // Group Address 2

    auto nowMs = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto& ac  = m_networkState.activeChannels;
        auto& dtg = m_networkState.discoveredTalkGroups;

        // Update active channels for both grants (retain encrypted flag if entry already exists)
        for (int i = 0; i < 2; ++i) {
            int ch = (i == 0) ? Channel1 : Channel2;
            int ga = (i == 0) ? GroupAddr1 : GroupAddr2;
            if (ga == 0) continue;

            auto it = std::find_if(ac.begin(), ac.end(),
                [ch, ga](const P25NetworkState::ActiveChannel& e) {
                    return e.channel == (uint16_t)ch && e.tgid == (uint16_t)ga;
                });
            if (it != ac.end()) {
                it->lastSeenMs = nowMs;
            } else {
                P25NetworkState::ActiveChannel entry{};
                entry.channel    = (uint16_t)ch;
                entry.tgid       = (uint16_t)ga;
                entry.lastSeenMs = nowMs;
                // ServiceOpts not present in GRANT_UPDT — encrypted status unknown; leave false.
                ac.push_back(entry);
            }
        }
        // Expire entries older than 10 s
        ac.erase(std::remove_if(ac.begin(), ac.end(),
            [nowMs](const P25NetworkState::ActiveChannel& e) {
                return (nowMs - e.lastSeenMs) > 10000;
            }), ac.end());

        for (int ga : { GroupAddr1, GroupAddr2 }) {
            if (ga == 0) continue;
            auto it = std::find_if(dtg.begin(), dtg.end(),
                [ga](const P25NetworkState::DiscoveredTalkGroup& e) { return e.tgid == (uint16_t)ga; });
            if (it != dtg.end()) {
                it->lastSeenMs = nowMs;
            } else {
                P25NetworkState::DiscoveredTalkGroup tg{};
                tg.tgid        = (uint16_t)ga;
                tg.firstSeenMs = nowMs;
                tg.lastSeenMs  = nowMs;
                tg.callCount   = 0;
                dtg.push_back(tg);
            }
        }
    }

    DSD_LOG("P25: TSBK Group Voice Channel Grant Update:"
            << " Ch1:0x" << std::hex << Channel1 << " TGID1:" << std::dec << GroupAddr1
            << " Ch2:0x" << std::hex << Channel2 << " TGID2:" << std::dec << GroupAddr2);
}

void DSDP25P1::processTimeDateAnnouncement(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 TIME_DATE_ANNC - Time and Date Announcement
    // Field bit layout (after args[0] = bits[79:72]):
    //   LGT[3:0]  = bits[79:76] → (args[0]>>4) & 0x0F  — Local GMT Offset; bit3=sign(1=E), bits2:0=hours
    //   Month[3:0]= bits[75:72] → args[0] & 0x0F
    //   DD[4:0]   = bits[71:67] → (args[1]>>3) & 0x1F
    //   YY[6:0]   = bits[66:60] → ((args[1]&0x07)<<4)|(args[2]>>4)  — 2-digit year, 2000-based
    //   HH[4:0]   = bits[59:55] → ((args[2]&0x0F)<<1)|(args[3]>>7)
    //   MN[5:0]   = bits[54:49] → (args[3]>>1) & 0x3F
    int LGT   = (tsbk.args[0] >> 4) & 0x0F;
    int Month = tsbk.args[0] & 0x0F;
    int Day   = (tsbk.args[1] >> 3) & 0x1F;
    int Year  = ((tsbk.args[1] & 0x07) << 4) | (tsbk.args[2] >> 4);
    int Hour  = ((tsbk.args[2] & 0x0F) << 1) | (tsbk.args[3] >> 7);
    int Min   = (tsbk.args[3] >> 1) & 0x3F;

    int lgtSign  = (LGT >> 3) & 0x1;   // 1 = East (positive), 0 = West (negative)
    int lgtHours = LGT & 0x07;

    //DSD_LOG("P25: TSBK Time/Date: "
    //        << (2000 + Year) << "-"
    //        << Month << "-" << Day << " "
    //        << Hour << ":" << Min
    //        << " UTC" << (lgtSign ? "+" : "-") << lgtHours);
}

void DSDP25P1::processIdenUpdateTDMA(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 IDEN_UP_TDMA - Channel Identifier Update for TDMA channels
    static const int slotsPerCarrier[] = {1,1,1,2,4,2,2,2,2,2,2,2,2,2,2,2};

    int Identifier  = (tsbk.args[0] >> 4) & 0x0F;                                          // Channel identifier (4 bits)
    int ChannelType = tsbk.args[0] & 0x0F;                                                  // TDMA channel type (4 bits)
    int Slots       = slotsPerCarrier[ChannelType];
    int TXOffset0   = (tsbk.args[1] << 6) | ((tsbk.args[2] >> 2) & 0x3F);                  // TX offset raw (14 bits)
    int Spacing     = ((tsbk.args[2] & 0x03) << 8) | tsbk.args[3];                         // Channel spacing (10 bits, units 125 Hz)
    long BaseFreq   = ((long)tsbk.args[4] << 24) | ((long)tsbk.args[5] << 16)
                    | ((long)tsbk.args[6] << 8)  |  tsbk.args[7];                           // Base frequency (32 bits, units 5 Hz)

    // Signed TX offset: bit 13 = sign (0 = negative, 1 = positive); magnitude in spacing×125 Hz units
    int  toff_sign  = (TXOffset0 >> 13) & 0x1;
    long TXOffsetHz = (long)(TXOffset0 & 0x1FFF) * Spacing * 125;
    if (toff_sign == 0)
        TXOffsetHz = -TXOffsetHz;

    //DSD_LOG("P25: TSBK IDEN Update TDMA: ID:" << Identifier
    //        << " Type:" << ChannelType << "(" << Slots << " slots)"
    //        << " BaseFreq:" << (BaseFreq * 5) << "Hz"
    //        << " Spacing:" << (Spacing * 125) << "Hz"
    //        << " TXOffset:" << TXOffsetHz << "Hz");

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto& ci = m_networkState.channelIdents[Identifier & 0x0F];
        ci.valid          = true;
        ci.baseFreqHz     = BaseFreq * 5;
        ci.spacingHz      = (int64_t)Spacing * 125;
        ci.txOffsetHz     = TXOffsetHz;
        ci.bwHz           = (int32_t)((int64_t)Spacing * 125);
        ci.slotsPerCarrier = Slots;
        ci.isTDMA         = (Slots > 1);
    }
}

void DSDP25P1::processIdenUpdate(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 IDEN_UP - Channel Identifier Update (wideband)
    int Identifier  = (tsbk.args[0] >> 4) & 0x0F;                                          // Channel identifier (4 bits)
    int BW          = ((tsbk.args[0] & 0x0F) << 5) | (tsbk.args[1] >> 3);                  // Bandwidth (9 bits, units 125 Hz)
    int TXOffset0   = ((tsbk.args[1] & 0x07) << 6) | (tsbk.args[2] >> 2);                  // TX offset raw (9 bits)
    int Spacing     = ((tsbk.args[2] & 0x03) << 8) | tsbk.args[3];                         // Channel spacing (10 bits, units 125 Hz)
    long BaseFreq   = ((long)tsbk.args[4] << 24) | ((long)tsbk.args[5] << 16)
                    | ((long)tsbk.args[6] << 8)  |  tsbk.args[7];                           // Base frequency (32 bits, units 5 Hz)

    // Signed TX offset: bit 8 = sign (0 = negative, 1 = positive); magnitude in 250 kHz units
    int  toff_sign  = (TXOffset0 >> 8) & 0x1;
    long TXOffsetHz = (long)(TXOffset0 & 0xFF) * 250000;
    if (toff_sign == 0)
        TXOffsetHz = -TXOffsetHz;

    //DSD_LOG("P25: TSBK IDEN Update: ID:" << Identifier
    //        << " BW:" << (BW * 125) << "Hz"
    //        << " BaseFreq:" << (BaseFreq * 5) << "Hz"
    //        << " Spacing:" << (Spacing * 125) << "Hz"
    //        << " TXOffset:" << TXOffsetHz << "Hz");

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto& ci = m_networkState.channelIdents[Identifier & 0x0F];
        ci.valid          = true;
        ci.baseFreqHz     = BaseFreq * 5;
        ci.spacingHz      = (int64_t)Spacing * 125;
        ci.txOffsetHz     = TXOffsetHz;
        ci.bwHz           = (int32_t)((int64_t)BW * 125);
        ci.slotsPerCarrier = 1;
        ci.isTDMA         = false;
    }
}

void DSDP25P1::processSecondaryControlChannelBroadcast(TSBK& tsbk)
{
    // TIA-102.AABC-B s6.2 SCCB - Secondary Control Channel Broadcast
    int RFSSID   = tsbk.args[0];                             // RF Subsystem ID
    int SiteID   = tsbk.args[1];                             // Site ID
    int Channel1 = (tsbk.args[2] << 8) | tsbk.args[3];      // Transmit channel
    // args[4] reserved
    int Channel2 = (tsbk.args[5] << 8) | tsbk.args[6];      // Receive channel

    //DSD_LOG("P25: TSBK Secondary Control Channel Broadcast: RFSSID:" << RFSSID
    //        << " SiteID:" << SiteID
    //        << " Ch1(Tx):0x" << std::hex << Channel1
    //        << " Ch2(Rx):0x" << Channel2);
}

void DSDP25P1::processTSBKOpcodeMoto(TSBK& tsbk)
{
    switch (tsbk.opcode)
    {
    case 0x00: // MOT_GRG_ADD_CMD - Group Regroup Add (supergroup patch)
    {
        int SuperGroup = (tsbk.args[0] << 8) | tsbk.args[1];   // Patch supergroup ID
        int GroupAddr1 = (tsbk.args[2] << 8) | tsbk.args[3];   // Patch group 1
        int GroupAddr2 = (tsbk.args[4] << 8) | tsbk.args[5];   // Patch group 2
        int GroupAddr3 = (tsbk.args[6] << 8) | tsbk.args[7];   // Patch group 3

        //DSD_LOG("P25: Moto Group Regroup Add: SuperGroup:" << SuperGroup
        //        << " GA1:" << GroupAddr1
        //        << " GA2:" << GroupAddr2
        //        << " GA3:" << GroupAddr3);
        break;
    }

    case 0x0B: // MOT_CC_BSI - Control Channel Base Station Identifier (callsign)
    {
        // 7 characters packed as 6-bit values (ASCII offset +43); 0 = pad/skip
        int chars[7];
        chars[0] = (tsbk.args[0] >> 2) & 0x3F;
        chars[1] = ((tsbk.args[0] & 0x03) << 4) | (tsbk.args[1] >> 4);
        chars[2] = ((tsbk.args[1] & 0x0F) << 2) | (tsbk.args[2] >> 6);
        chars[3] =   tsbk.args[2] & 0x3F;
        chars[4] = (tsbk.args[3] >> 2) & 0x3F;
        chars[5] = ((tsbk.args[3] & 0x03) << 4) | (tsbk.args[4] >> 4);
        chars[6] = ((tsbk.args[4] & 0x0F) << 2) | (tsbk.args[5] >> 6);
        int Channel = (tsbk.args[6] << 8) | tsbk.args[7];

        std::string callsign;
        for (int i = 0; i < 7; i++)
            if (chars[i] != 0)
                callsign += static_cast<char>(chars[i] + 43);

        //DSD_LOG("P25: Moto Base Station ID: Callsign:\"" << callsign
        //        << "\" Ch:0x" << std::hex << Channel);
        break;
    }

    default: // Unknown Motorola MFID 0x90 opcode — log raw bytes
    {
        /*
        std::ostringstream _oss;
        _oss << "P25: Moto Unknown opcode:0x" << std::hex << (int)tsbk.opcode << " args:";
        for (int i = 0; i < 8; i++)
            _oss << " " << std::hex << (int)tsbk.args[i];
        dsd_trace_post(_oss.str().c_str());
        */
        break;
    }
    }
}


void DSDP25P1::processPDU()
{
    // Packet Data Unit - data transmission
    //TRACE("P25: PDU processing not implemented\n");
    m_dsdDecoder->resetFrameSync();
}

int DSDP25P1::find_min(uint8_t list[], int len)
{
    int min = list[0];
    int index = 0;
    int i;

    for (i = 1; i < len; i++) {
        if (list[i] < min) {
            min = list[i];
            index = i;
        }
    }
    // On a tie, return the first minimum found (best-effort; CRC validates the result).
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