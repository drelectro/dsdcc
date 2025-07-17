///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Mike Cornelius, VK2XMC.                                    //       
// Based on the work of Edouard Griffiths, F4EXB.                                //
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

#ifndef DSDCC_P25P1_H_
#define DSDCC_P25P1_H_

#include "fec.h"
#include "viterbi.h"
#include "crc.h"
#include "reed_solomon.h"
#include "p25p1_heuristics.h"
#include "export.h"

namespace DSDcc
{

class DSDDecoder;

class DSDCC_API DSDP25P1
{
public:
    typedef enum
    {
        P25P1FrameNone,
        P25P1FrameLDU1,
        P25P1FrameLDU2,
        P25P1FrameTDU,
        P25P1FrameTDULC,
        P25P1FrameTSBK,
        P25P1FramePDU,
        P25P1FrameHDU
    } P25P1FrameType;

    typedef enum
    {
        P25P1StateNone,
        P25P1StateHDU,
        P25P1StateStartFrame,
        P25P1StateFrame,
        P25P1StateFramePayload
    } P25P1State;

    DSDP25P1(DSDDecoder *dsdDecoder);
    ~DSDP25P1();

    void init();
    void process();
    void processHDU();

    P25P1FrameType getFrameType() const { return m_frameType; }
    bool isEncrypted() const { return m_encrypted; }
    unsigned int getNAC() const { return m_nac; }
    unsigned int getDUID() const { return m_duid; }
    unsigned int getTalkGroup() const { return m_talkGroup; }
    unsigned int getSource() const { return m_source; }
    unsigned int getAlgId() const { return m_algId; }
    unsigned int getKeyId() const { return m_keyId; }
    const char* getEmergencyText() const { return m_emergency ? "EMERGENCY" : ""; }

private:
    void processNID();
    void processFramePayload();
    void processLDU1();
    void processLDU2();
    void processTDU();
    void processTDULC();
    void processTSBK();
    void processPDU();
    
    void processVoiceFrame(int frameIndex);
    void processLCFrame();
    void processStatusFrame();
    void processESFrame();
    
    void extractIMBE(unsigned char* imbeFrame, int frameIndex);
    void extractLinkControl();
    void extractStatusSymbols();
    void extractEncryptionSync();
    
    bool decodeHamming_10_6_3(unsigned char* data);
    bool decodeGolay_24_12_8(unsigned char* data);
    bool decodeReedSolomon_24_12_13(unsigned char* data, int blocks);
    bool decodeTrellis_3_4(unsigned char* data, int length);
    
    void storeAnalogSignal(int value, int dibit);
    void processHeuristics();

    DSDDecoder *m_dsdDecoder;
    
    // Frame state
    P25P1FrameType m_frameType;
    P25P1State m_state;
    int m_symbolIndex;
    int m_frameIndex;
    bool m_encrypted;
    bool m_emergency;
    
    // P25 identifiers
    unsigned int m_nac;          // Network Access Code
    unsigned int m_duid;         // Data Unit ID
    unsigned int m_talkGroup;
    unsigned int m_source;
    unsigned int m_algId;        // Algorithm ID
    unsigned int m_keyId;        // Key ID
    unsigned int m_mfId;         // Manufacturer ID
    
    // Frame data buffers
    unsigned char m_nidData[8];
    unsigned char m_lcData[12];
    unsigned char m_esData[4];
    unsigned char m_rsData[24];
    unsigned char m_statusData[18];
    
    // Voice data
    unsigned char m_imbeFrame[18][11];  // 18 voice frames per LDU
    int m_imbeFrameIndex;
    
    // Heuristics and FEC
    DSDP25Heuristics::AnalogSignal m_analogSignalArray[400];
    int m_analogSignalIndex;
    Hamming_10_6_3 m_hamming_10_6_3;
    Golay_24_12_8 m_golay_24_12_8;
    ReedSolomon_24_12_13 m_reedSolomon_24_12_13;
    Viterbi m_viterbi_3_4;
    CRC m_crcP25;
    
    // Constants
    static const int m_imbeMap[18][11];
    static const int m_statusMap[18];
    static const int m_lcMap[12];
    static const int m_esMap[4];
};

} // namespace DSDcc

#endif /* DSDCC_P25P1_H_ */