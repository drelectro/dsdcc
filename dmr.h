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

#ifndef DMR_H_
#define DMR_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

#include "fec.h"
#include "export.h"

#define DMR_TYPES_COUNT 16
#define DMR_SLOT_TYPE_PARITY_LEN 12

#define DMR_TS_LEN 288              //!< Length of 30ms frame in bits
#define DMR_CACH_LEN 24
#define DMR_SYNC_LEN 48

#define DMR_EMB_PART_LEN 8
#define DMR_ES_LEN 32               //!< Embedded signaling length
#define DMR_SLOT_TYPE_PART_LEN 10

#define DMR_VOCODER_FRAME_LEN 72    //!< Size in bits of Vocoder frame (20ms)
#define DMR_VOX_PART_LEN 108
#define DMR_DATA_PART_LEN 98

#define DMR_VOX_SUPERFRAME_LEN 6    //!< Count of frames in Superframe
#define IN_DIBITS(x) ((x)/2)        //!< Convert size to dibit units count
#define IN_BYTES(x) ((x)/8)
#define DMR_BP_KEYS_COUNT 255   //!< Basic Privacy Keys count

typedef enum
{
    SingleLC_FirstCSBK,      // 0
    FirstLC,                 // 1
    LastLC_CSBK,             // 2
    ContLC_CSBK              // 3
} DMRLcss;

namespace DSDcc
{

class DSDDecoder;

class DSDCC_API DSDDMR
{
public:
    typedef enum
    {
        DSDDMRBurstNone,
        DSDDMRBaseStation,       //!< BS sourced voice or data
        DSDDMRMobileStation,     //!< MS sourced voice or data
        DSDDMRMobileStationRC,   //!< MS sourced standalone RC
        DSDDMRDirectSlot1,       //!< TDMA direct mode time slot 1 voice or data
        DSDDMRDirectSlot2        //!< TDMA direct mode time slot 2 voice or data
    } DSDDMRBurstType;

    typedef enum
    {
        DSDDMRSlot1,             // 0
        DSDDMRSlot2,             // 1
        DSDDMRSlotUndefined      // 2
    } DSDDMRSlot;

    typedef enum
    {
        DSDDMRDataPIHeader,         // 0
        DSDDMRDataVoiceLCHeader,    // 1
        DSDDMRDataTerminatorWithLC, // 2
        DSDDMRDataCSBK,             // 3
        DSDDMRDataMBCHeader,        // 4
        DSDDMRDataMBCContinuation,  // 5
        DSDDMRDataDataHeader,       // 6
        DSDDMRDataRate_1_2_Data,    // 7
        DSDDMRDataRate_3_4_Data,    // 8
        DSDDMRDataIdle,             // 9
        DSDDMRDataRate_1,           // 10
        DSDDMRDataUnifiedSingleBlock, // 11
        DSDDMRDataReserved,
        DSDDMRDataUnknown
    } DSDDMRDataTYpe;

    // Snapshot of decoded DMR Tier III trunking state — safe to read from any thread
    // via getNetworkStateCopy(). Populated from CRC-OK TSCC CSBKs/MBCs only.
    struct DMRNetworkState
    {
        // TSCC identity — from C_ALOHA / C_BCAST System Identity Code
        bool     tsccDetected = false;
        uint64_t lastTsccActivityMs = 0; //!< steady_clock ms of last CRC-OK TSCC CSBK
        uint16_t sysCode = 0;            //!< raw 16-bit C_SYScode
        uint8_t  sysModel = 0;           //!< 0=Tiny 1=Small 2=Large 3=Huge
        uint16_t netId = 0;
        uint16_t siteId = 0;
        uint8_t  par = 0;                //!< TSCC slot usage (categories A/B)
        uint8_t  colorCode = 0;
        uint8_t  vendorFid = 0;          //!< last recognised manufacturer FID (DMRA MFID)

        // Logical channel → frequency mappings learned over the air
        // (C_BCAST Chan_Freq announcements, CSBK and MBC forms)
        struct LearnedChannel {
            uint16_t lpcn = 0;           //!< 12-bit logical physical channel number
            int64_t  rxFreqHz = 0;       //!< MS receive (BS transmit / downlink)
            int64_t  txFreqHz = 0;       //!< MS transmit (0 if not announced)
            uint64_t lastSeenMs = 0;
        };
        std::vector<LearnedChannel> learnedChannels;

        // Adjacent / vote-target sites from C_BCAST announcements
        struct AdjacentSite {
            uint16_t sysCode = 0;
            uint16_t lpcn = 0;           //!< that site's TSCC channel (0 if unknown)
            bool     voteNow = false;    //!< true if learned from a Vote_Now advice
            uint64_t lastSeenMs = 0;
        };
        std::vector<AdjacentSite> adjacentSites;

        // Active calls from PV/TV/BTV/PD/TD grants; consumers skip entries older than ~10 s
        struct ActiveCall {
            uint16_t lpcn = 0;
            int      slot = 0;           //!< 0 or 1, from the grant TS bit
            uint32_t tgid = 0;           //!< 24-bit destination address
            uint32_t srcAddr = 0;        //!< 24-bit source address
            bool     isGroup = true;     //!< false for PV/PD (individual) grants
            bool     isData = false;     //!< true for PD/TD (data) grants
            bool     emergency = false;
            uint64_t lastSeenMs = 0;
        };
        std::vector<ActiveCall> activeCalls;

        // Talk groups observed on group voice grants; never expires
        struct DiscoveredTalkGroup {
            uint32_t tgid = 0;
            uint64_t firstSeenMs = 0;
            uint64_t lastSeenMs = 0;
            uint32_t callCount = 0;
        };
        std::vector<DiscoveredTalkGroup> discoveredTalkGroups;

        // Cumulative counters — not reset on sync loss
        uint32_t csbkTotalCount = 0;
        uint32_t csbkCrcOkCount = 0;
        uint32_t csbkCrcFailCount = 0;
        uint32_t mbcAssembledCount = 0;
        uint32_t mbcCrcFailCount = 0;
        uint32_t bptcFailCount = 0;
    };

    DMRNetworkState getNetworkStateCopy() const
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return m_networkState;
    }

    // Discard all accumulated network state (identity, learned channel plan,
    // calls, talkgroups, counters). Called from the UI thread when the control
    // receiver is retuned — the old system's data is stale. An in-flight MBC
    // assembly is DSP-thread-owned (not mutex-protected) and self-aborts via
    // its 720 ms staleness timeout, so it is deliberately not touched here.
    void resetNetworkState()
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_networkState = DMRNetworkState();
    }

    void setVerbosity(int verbosity) { m_verbosity = verbosity; }

    // DMRA Manufacturer's ID (FID) → vendor name, or nullptr for the ETSI-standard
    // FID (0x00) and unrecognised codes. Shared by the bench and the status widget.
    static const char *fidVendorName(unsigned char fid);

    explicit DSDDMR(DSDDecoder *dsdDecoder);
    ~DSDDMR();

    void initData();
    void initVoice();
    void processData();
    void processVoice();
    void processSyncOrSkip();

    void initVoiceMS();
    void processVoiceMS();
    void processSkipMS();
    void initDataMS();
    void processDataMS();

    const char *getSlot0Text() const;
    const char *getSlot1Text() const;
    unsigned char getColorCode() const;

private:
    struct DMRAddresses
    {
        DMRAddresses() :
            m_group(false),
            m_target(0),
            m_source(0)
        {
        }

        bool         m_group;
        unsigned int m_target;
        unsigned int m_source;
    };

    void processDataFirstHalf(unsigned int shiftBack);  //!< Because sync is in the middle of a frame you need to process the first half first: CACH to end of SYNC
    void processVoiceFirstHalf(unsigned int shiftBack); //!< Because sync is in the middle of a frame you need to process the first half first: CACH to end of SYNC
    void decodeCACH(unsigned char *cachBits);
    void processSlotTypePDU();
    bool processEMB();
    bool processVoiceEmbeddedSignalling(int& voiceEmbSig_dibitsIndex, unsigned char *voiceEmbSigRawBits, bool& voiceEmbSig_OK, DMRAddresses& addresses);
    void processVoiceDibit(unsigned char dibit);
    void processDataDibit(unsigned char dibit);
    void storeSymbolDV(unsigned char *mbeFrame, int dibitindex, unsigned char dibit, bool invertDibit = false);
    static void textVoiceEmbeddedSignalling(DMRAddresses& addresses, char *slotText);

    void processVoiceFirstHalfMS();
    void processDataFirstHalfMS();

    void BasicPrivacyXOR(unsigned char *dibit, int pos);

    bool decodeBPTC196_96(unsigned char *infoBits);         //!< BPTC(196,96) decode of m_dataDibits → 96 info bits
    void decodeCSBK(const unsigned char *infoBits);         //!< Parse CSBK PDU (ETSI TS 102 361-1 §9.1.7)
    void decodeMBCHeader(const unsigned char *infoBits);    //!< Parse MBC Header PDU, start per-slot assembly
    void decodeMBCContinuation(const unsigned char *infoBits); //!< Append MBC Continuation block, dispatch on last block
    void processAssembledMBC(int slotIdx);                  //!< Decode a fully assembled multi-block CSBK
    void noteCSBKSyncAcquired();
    void noteCSBKSyncLost();
    bool shouldLogCSBK(unsigned char csbko, unsigned char mfid, bool crcOK, const std::string& messageText);

    // Per-opcode CSBK payload parsers (TS 102 361-4). Each appends decoded fields to msg
    // and, when updateState is true (CRC OK), mutates m_networkState under m_stateMutex.
    // contBits/nContBits carry MBC continuation payload bits for the MBC forms (null for CSBK form).
    void parseCSBKPayload(unsigned char csbko, unsigned char mfid, const unsigned char *infoBits,
                          std::ostringstream& msg, bool updateState,
                          const unsigned char *contBits = nullptr, int nContBits = 0);
    void parseAloha(const unsigned char *infoBits, std::ostringstream& msg, bool updateState);
    void parseAhoyOrRand(const unsigned char *infoBits, std::ostringstream& msg);
    void parseGrant(unsigned char csbko, const unsigned char *infoBits, std::ostringstream& msg, bool updateState);
    void parseBcast(const unsigned char *infoBits, std::ostringstream& msg, bool updateState,
                    const unsigned char *contBits, int nContBits);
    void parseMove(const unsigned char *infoBits, std::ostringstream& msg);
    void noteTsccIdentity(unsigned int sic);                //!< Update site identity in state (caller holds no lock)

    DSDDecoder *m_dsdDecoder;
    int  m_symbolIndex;                   //!< current symbol index in non HD sequence
    int  m_cachSymbolIndex;               //!< count of symbols since last positive CACH identification
    DSDDMRBurstType m_burstType;
    DSDDMRSlot m_slot;
    bool m_continuation;
    bool m_cachOK;
    unsigned char m_lcss;
    unsigned char m_colorCode;
    DSDDMRDataTYpe m_dataType;
    char *m_slotText;
    unsigned char m_slotTypePDU_dibits[10];
    unsigned char m_cachBits[24];
    unsigned char m_emb_dibits[8];
    unsigned char m_voiceEmbSig_dibits[16];
    unsigned char m_voice1EmbSigRawBits[16*8];
    int           m_voice1EmbSig_dibitsIndex;
    bool          m_voice1EmbSig_OK;
    DMRAddresses  m_slot1Addresses;
    unsigned char m_voice2EmbSigRawBits[16*8];
    int           m_voice2EmbSig_dibitsIndex;
    bool          m_voice2EmbSig_OK;
    DMRAddresses  m_slot2Addresses;
    unsigned char m_syncDibits[24];
    unsigned char m_dataDibits[98];          //!< Data payload buffer: first half [0..48] + second half [49..97] = 196 bits for BPTC decode
    unsigned int m_voice1FrameCount; //!< current frame count in voice superframe: [0..5] else no superframe on going
    unsigned int m_voice2FrameCount; //!< current frame count in voice superframe: [0..5] else no superframe on going
    unsigned char m_mbeDVFrame[9];

    Hamming_7_4 m_hamming_7_4;
    Golay_20_8 m_golay_20_8;
    QR_16_7_6 m_qr_16_7_6;
    Hamming_16_11_4 m_hamming_16_11_4;
    Hamming_15_11 m_hamming_15_11;

    const int *w, *x, *y, *z;

    static const int m_cachInterleave[24];
    static const int m_embSigInterleave[128];
    static const char *m_slotTypeText[DMR_TYPES_COUNT];

    static const int rW[36];
    static const int rX[36];
    static const int rY[36];
    static const int rZ[36];
    static const unsigned short BasicPrivacyKeys[DMR_BP_KEYS_COUNT];

    struct CSBKLogState
    {
        std::size_t signature = 0;
        std::uint64_t lastLogMs = 0;
        std::uint32_t syncEpoch = 0;
        bool seen = false;
    };

    std::unordered_map<std::uint32_t, CSBKLogState> m_csbkLogStates;
    std::uint32_t m_csbkSyncEpoch = 0;
    bool m_csbkSyncLocked = false;

    // Multi-block CSBK assembly, one per slot (slots interleave burst by burst)
    struct MBCAssembly
    {
        bool          active = false;
        unsigned char csbko = 0;
        unsigned char mfid = 0;
        unsigned char blockBits[8][96];  //!< info bits per block: [0]=header, [1..]=continuations
        int           numBlocks = 0;
        std::uint64_t startMs = 0;
    };
    MBCAssembly m_mbcAssembly[2];

    DMRNetworkState m_networkState;
    mutable std::mutex m_stateMutex;

    int m_verbosity = 1;
};

} // namespace DSDcc



#endif /* DMR_H_ */
