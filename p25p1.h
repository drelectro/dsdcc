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

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

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

    // Structures
    struct TSBK
    {
        TSBK() :
            opcode(0),
            mfId(0),
            args{ 0 },
            crc(0)
        {
        }
        unsigned char opcode;   //!< TSBK Last block (1 bit) Protetcted (1 bit) opcode (6 bits)
        unsigned char mfId;     //!< Manufacturer ID (8 bits)
        unsigned char args[8];  //!< Arguments (64 bits total)
        unsigned short crc;     //!< CRC-16 (16 bits)
    };

    // Snapshot of decoded P25 network state — safe to read from any thread via getNetworkStateCopy().
    struct P25NetworkState
    {
        // From NID (decoded on every frame)
        uint16_t nac = 0;

        // From NET_STS_BCST (0x3B)
        uint8_t  lra = 0;
        uint32_t wacnId = 0;
        uint16_t netSystemId = 0;
        uint16_t netChannel = 0;
        uint8_t  netServiceClass = 0;

        // From RFSS_STS_BCST (0x3A)
        uint8_t  rfssFlags = 0;       //!< bit7=ROAM, bit6=ELK
        uint16_t rfssSystemId = 0;
        uint8_t  rfssId = 0;
        uint8_t  siteId = 0;
        uint16_t rfssChannel = 0;
        uint8_t  rfssServiceClass = 0;

        // Channel identifier table entries (from IDEN_UP_*, indexed 0-15 by identifier field)
        struct ChannelIdent {
            bool    valid = false;
            int64_t baseFreqHz = 0;
            int64_t spacingHz = 0;
            int64_t txOffsetHz = 0;
            int32_t bwHz = 0;           //!< Bandwidth (0 for VU type — use spacingHz)
            int     slotsPerCarrier = 1;
            bool    isTDMA = false;
        };
        ChannelIdent channelIdents[16] = {};

        // Active voice channels from GRP_V_CH_GRANT (0x00); expired after 10 s of inactivity
        struct ActiveChannel {
            uint16_t channel = 0;
            uint16_t tgid = 0;
            uint32_t srcAddr = 0;
            uint64_t lastSeenMs = 0;    //!< std::chrono::steady_clock milliseconds since epoch
            bool     encrypted = false; //!< ServiceOpts bit 6 from channel grant
        };
        std::vector<ActiveChannel> activeChannels;

        // All talk groups observed on channel grants; never expires
        struct DiscoveredTalkGroup {
            uint16_t tgid = 0;
            uint64_t firstSeenMs = 0;
            uint64_t lastSeenMs = 0;
            uint32_t callCount = 0;
            bool     encrypted = false; //!< true if the most recent grant for this TG was encrypted
        };
        std::vector<DiscoveredTalkGroup> discoveredTalkGroups;

        // Cumulative counters — not reset on sync loss
        uint32_t tsbkTotalCount = 0;
        uint32_t crcOkCount = 0;
        uint32_t crcFailCount = 0;
        uint32_t trellisFailCount = 0;
    };

    P25NetworkState getNetworkStateCopy() const
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return m_networkState;
    }


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

    void processTSBKOpcode(TSBK& tsbk);
    void processTSBKOpcodeMoto(TSBK& tsbk);
    void processNetworkStatusBroadcast(TSBK& tsbk);
    void processGroupVoiceChannelGrant(TSBK& tsbk);
    void processGroupVoiceChannelGrantUpdate(TSBK& tsbk);
    void processTimeDateAnnouncement(TSBK& tsbk);
    void processIndividualDataRequest(TSBK& tsbk);
    void processIdenUpdateTDMA(TSBK& tsbk);
    void processIdenUpdateVU(TSBK& tsbk);
    void processIdenUpdate(TSBK& tsbk);
    void processSecondaryControlChannelBroadcast(TSBK& tsbk);
    void processRFSSStatusBroadcast(TSBK& tsbk);
    void processAdjacentStatusBroadcast(TSBK& tsbk);
    
    void extractIMBE(unsigned char* imbeFrame, int frameIndex);
    void extractLinkControl();
    void extractStatusSymbols();
    void extractEncryptionSync();
    
    bool decodeBCH_63_16_5(unsigned char* data);
    bool decodeHamming_10_6_3(unsigned char* data);
    bool decodeGolay_24_12_8(unsigned char* data);
    bool decodeReedSolomon_24_12_13(unsigned char* data, int blocks);
    bool decodeTrellis_3_4(unsigned char* data, int length);
    bool decodeTrellis_1_2(); 
    
    void storeAnalogSignal(int value, int dibit);
    void processHeuristics();

    int count_bits(unsigned int n);
    int find_min(uint8_t list[], int len);

    DSDDecoder *m_dsdDecoder;
    
    // Frame state
    P25P1FrameType m_frameType;
    P25P1State m_state;
    int m_symbolIndex;
    int m_frameIndex;
    bool m_encrypted;
    bool m_emergency;
	int _symbolsExpected;        // Number of dibits to acquire before next processing step
	int _statusIndex;            // Index for status symbol removal and processing   
    
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
	unsigned char _deinterleavedDibits[98];  
	unsigned char _frameData[128];   
    
    // Voice data
    unsigned char m_imbeFrame[18][11];  // 18 voice frames per LDU
    int m_imbeFrameIndex;
    
    // Heuristics and FEC
    DSDP25Heuristics::AnalogSignal m_analogSignalArray[400];
    int m_analogSignalIndex;
    Hamming_10_6_3 m_hamming_10_6_3;
    BCH_63_16_5 m_bch_63_16_5;
    Golay_24_12_8 m_golay_24_12_8;
    ReedSolomon_24_12_13 m_reedSolomon_24_12_13;
    Viterbi m_viterbi_3_4;
    Viterbi m_viterbi_1_2;  
    CRC m_crcP25;
    
    // Constants
    static const int m_imbeMap[18][11];
    static const int m_statusMap[18];
    static const int m_lcMap[12];
    static const int m_esMap[4];
	static const unsigned int _dataPktMap[98];  //!< Deinterleave table for data and TSBK frames, ref BAAA 7.2

    // Network state (written on DSP thread, read via getNetworkStateCopy() under mutex)
    mutable std::mutex m_stateMutex;
    P25NetworkState    m_networkState;

    
    

};

} // namespace DSDcc

#endif /* DSDCC_P25P1_H_ */