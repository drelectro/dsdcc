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

#include <iostream>
#include <chrono>
#include <functional>
#include <iomanip>
#include <string.h>
#include "dmr.h"
#include "dsd_decoder.h"
#include "dsd_sync.h"

#pragma warning(disable : 4996)

namespace DSDcc
{

const int DSDDMR::m_cachInterleave[24]   = {0, 7, 8, 9, 1, 10, 11, 12, 2, 13, 14, 15, 3, 16, 4, 17, 18, 19, 5, 20, 21, 22, 6, 23};
const int DSDDMR::m_embSigInterleave[128] = {
        0,  16,  32,  48,  64,  80,  96, 112,
        1,  17,  33,  49,  65,  81,  97, 113,
        2,  18,  34,  50,  66,  82,  98, 114,
        3,  19,  35,  51,  67,  83,  99, 115,
        4,  20,  36,  52,  68,  84, 100, 116,
        5,  21,  37,  53,  69,  85, 101, 117,
        6,  22,  38,  54,  70,  86, 102, 118,
        7,  23,  39,  55,  71,  87, 103, 119,
        8,  24,  40,  56,  72,  88, 104, 120,
        9,  25,  41,  57,  73,  89, 105, 121,
       10,  26,  42,  58,  74,  90, 106, 122,
       11,  27,  43,  59,  75,  91, 107, 123,
       12,  28,  44,  60,  76,  92, 108, 124,
       13,  29,  45,  61,  77,  93, 109, 125,
       14,  30,  46,  62,  78,  94, 110, 126,
       15,  31,  47,  63,  79,  95, 111, 127,
};
//ETSI TS 102 361-1 9.3.6. Data Type
const char *DSDDMR::m_slotTypeText[DMR_TYPES_COUNT] = {
        "PIH",
        "VLC",
        "TLC",
        "CSB",
        "MBH",
        "MBC",
        "DAH",
        "D12",
        "D34",
        "IDL",
        "D01",
        "USB",
        "RSV",
        "RSV",
        "RSV",
        "RSV"
};


/*
 * DMR AMBE interleave schedule
 */
// bit 1
const int DSDDMR::rW[36] = {
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

const int DSDDMR::rX[36] = {
  23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

// bit 0
const int DSDDMR::rY[36] = {
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

const int DSDDMR::rZ[36] = {
  5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};

// MotoTRBO algo only. Hytera don`t use hardcoded keys
const unsigned short DSDDMR::BasicPrivacyKeys[DMR_BP_KEYS_COUNT] = {
  0x1F00, 0xE300, 0xFC00, 0x2503, 0x3A03, 0xC603, 0xD903, 0x4A05, 0x5505, 0xA905,
  0xB605, 0x6F06, 0x7006, 0x8C06, 0x9306, 0x2618, 0x3918, 0xC518, 0xDA18, 0x031B,
  0x1C1B, 0xE01B, 0xFF1B, 0x6C1D, 0x731D, 0x8F1D, 0x901D, 0x491E, 0x561E, 0xAA1E,
  0xB51E, 0x4B28, 0x5428, 0xA828, 0xB728, 0x6E2B, 0x712B, 0x8D2B, 0x922B, 0x012D,
  0x1E2D, 0xE22D, 0xFD2D, 0x242E, 0x3B2E, 0xC72E, 0xD82E, 0x6D30, 0x7230, 0x8E30,
  0x9130, 0x4833, 0x5733, 0xAB33, 0xB433, 0x2735, 0x3835, 0xC435, 0xDB35, 0x0236,
  0x1D36, 0xE136, 0xFE36, 0x2B49, 0x3449, 0xC849, 0xD749, 0x0E4A, 0x114A, 0xED4A,
  0xF24A, 0x614C, 0xAE4C, 0x824C, 0x9D4C, 0x444F, 0x5B4F, 0xA74F, 0xB84F, 0x0D51,
  0x1251, 0xEE51, 0xF151, 0x2852, 0x3752, 0xCB52, 0xD452, 0x4754, 0x5854, 0xA454,
  0xBB54, 0x6257, 0x7D57, 0x8157, 0x9E57, 0x6061, 0x7F61, 0x8361, 0x9C61, 0x4562,
  0x5A62, 0xA662, 0xB962, 0x2A64, 0x3564, 0xC964, 0xD664, 0x0F67, 0x1067, 0xEC67,
  0xF367, 0x4679, 0x5979, 0xA579, 0xBA79, 0x637A, 0x7C7A, 0x807A, 0x9F7A, 0x0C7C,
  0x137C, 0xEF7C, 0xF07C, 0x297F, 0x367F, 0xCA7F, 0xD57F, 0x4D89, 0x5289, 0xAE89,
  0xB189, 0x688A, 0x778A, 0x8B8A, 0x948A, 0x078C, 0x188C, 0xE48C, 0xFB8C, 0x228F,
  0x3D8F, 0xC18F, 0xDE8F, 0x6B91, 0x7491, 0x8891, 0x9791, 0x4E92, 0x5192, 0xAD92,
  0xB292, 0x2194, 0x3E94, 0xC294, 0xDD94, 0x0497, 0x1B97, 0xE797, 0xF897, 0x06A1,
  0x19A1, 0xE5A1, 0xFAA1, 0x23A2, 0x3CA2, 0xC0A2, 0xDFA2, 0x4CA4, 0x53A4, 0xAFA4,
  0xB0A4, 0x69A7, 0x76A7, 0x8AA7, 0x95A7, 0x20B9, 0x3FB9, 0xC3B9, 0xDCB9, 0x05BA,
  0x1ABA, 0xE6BA, 0xF9BA, 0x6ABC, 0x75BC, 0x89BC, 0x96BC, 0x4FBF, 0x50BF, 0xACBF,
  0xB3BF, 0x66C0, 0x79C0, 0x85C0, 0x9AC0, 0x43C3, 0x5CC3, 0xA0C3, 0xBFC3, 0x2CC5,
  0x33C5, 0xCFC5, 0x0DC0, 0x09C6, 0x16C6, 0xEAC6, 0xF5C6, 0x84D0, 0x85DF, 0x8AD3,
  0x8BDC, 0xB6D5, 0xB7DA, 0xB8D6, 0xB9D9, 0x0DDA, 0xD1D5, 0xDED9, 0xDFD6, 0xE2DF,
  0xE3D0, 0xECDC, 0xEDD3, 0x2DE8, 0x32E8, 0xCEE8, 0xD1E8, 0x08EB, 0x17EB, 0xEBEB,
  0xF4EB, 0x67ED, 0x78ED, 0x84ED, 0x9BED, 0x42EE, 0x5DEE, 0xA1EE, 0xBEEE, 0x0BF0,
  0x14F0, 0xE8F0, 0xF7F0, 0x2EF3, 0x31F3, 0xCDF3, 0xD2F3, 0x41F5, 0x5EF5, 0xA2F5,
  0xBDF5, 0x64F6, 0x7BF6, 0x87F6, 0x98F6
};
// ========================================================================================

DSDDMR::DSDDMR(DSDDecoder *dsdDecoder) :
        m_dsdDecoder(dsdDecoder),
        m_symbolIndex(0),
        m_cachSymbolIndex(0),
        m_burstType(DSDDMRBurstNone),
        m_slot(DSDDMRSlotUndefined),
        m_continuation(false),
        m_cachOK(false),
        m_lcss(SingleLC_FirstCSBK),
        m_colorCode(0),
        m_dataType(DSDDMRDataUnknown),
        m_voice1EmbSig_dibitsIndex(0),
        m_voice1EmbSig_OK(false),
        m_voice2EmbSig_dibitsIndex(0),
        m_voice2EmbSig_OK(false),
        m_voice1FrameCount(DMR_VOX_SUPERFRAME_LEN),
        m_voice2FrameCount(DMR_VOX_SUPERFRAME_LEN)
{
    m_slotText = m_dsdDecoder->m_state.slot0light;
    w = 0;
    x = 0;
    y = 0;
    z = 0;

    memset(m_slotTypePDU_dibits, 0, 10);
    memset(m_cachBits, 0, 24);
    memset(m_emb_dibits, 0, 8);
    memset(m_voiceEmbSig_dibits, 0, 16);
    memset(m_voice1EmbSigRawBits, 0, 16*8);
    memset(m_voice2EmbSigRawBits, 0, 16*8);
    memset(m_syncDibits, 0, 24);
    memset(m_mbeDVFrame, 0, 9);
    memset(m_dataDibits, 0, 98);
}

DSDDMR::~DSDDMR()
{
}

void DSDDMR::initData()
{
//    DSD_LOG("DSDDMR::initData");
    noteCSBKSyncAcquired();
    m_burstType = DSDDMRBaseStation;
    processDataFirstHalf(90+1);
}

void DSDDMR::initDataMS()
{
//    DSD_LOG("DSDDMR::initDataMS");
    noteCSBKSyncAcquired();
    m_burstType = DSDDMRMobileStation;
    processDataFirstHalfMS();
}

void DSDDMR::initVoice()
{
//    DSD_LOG("DSDDMR::initVoice");
    m_burstType = DSDDMRBaseStation;
    processVoiceFirstHalf(90+1);
}

void DSDDMR::initVoiceMS()
{
//    DSD_LOG("DSDDMR::initVoiceMS");
    m_burstType = DSDDMRMobileStation;
    processVoiceFirstHalfMS();
}

void DSDDMR::processData()
{
    if ((!m_cachOK) && (m_burstType == DSDDMRBaseStation))
    {
        m_slotText = m_dsdDecoder->m_state.slot0light;
        memcpy(m_dsdDecoder->m_state.slot0light, "/-- UNK", 7);
        m_dsdDecoder->resetFrameSync();
        return; // abort
    }

    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit(); // get dibit from symbol

    processDataDibit(dibit);

    if (m_symbolIndex == IN_DIBITS(DMR_TS_LEN) - 1) // last dibit
    {
        if (m_slot == DSDDMRSlot1)
        {
            if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN) // continuation expected on slot + 2
            {
                DSD_LOG("DSDDMR::processData: error: remaining voice in slot1");

                if (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 2
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRsyncOrSkip; // sync lookup or skip in slot 2
                    m_continuation = false; // TODO: true or false?
                }
            }
            else
            {
                if (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 2
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->resetFrameSync(); // back to sync
                    m_continuation = false;
                }
            }
        }
        else if (m_slot == DSDDMRSlot2)
        {
            if (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN) // continuation expected on slot + 2
            {
                DSD_LOG("DSDDMR::processData: error: remaining voice in slot2");

                if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 1
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRsyncOrSkip; // sync lookup or skip in slot 1
                    m_continuation = false; // TODO: true or false?
                }
            }
            else
            {
                if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 1
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->resetFrameSync(); // back to sync
                    m_continuation = false;
                }
            }
        }

        m_symbolIndex = 0;
    }
    else
    {
        m_symbolIndex++;
    }

    m_cachSymbolIndex++; // last dibit counts
}

void DSDDMR::processDataMS()
{
    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit(); // get dibit from symbol

    processDataDibit(dibit);

    if (m_symbolIndex == IN_DIBITS(DMR_TS_LEN) - 1) // last dibit
    {
        m_dsdDecoder->resetFrameSync(); // back to sync
        m_symbolIndex = 0;
    }
    else
    {
        m_symbolIndex++;
    }
}

void DSDDMR::processVoice()
{
    if ((!m_cachOK) && (m_burstType == DSDDMRBaseStation))
    {
        m_slotText = m_dsdDecoder->m_state.slot0light;
        memcpy(m_dsdDecoder->m_state.slot0light, "/-- UNK", 7);
        m_voice1FrameCount = DMR_VOX_SUPERFRAME_LEN;
        m_voice2FrameCount = DMR_VOX_SUPERFRAME_LEN;
        m_dsdDecoder->resetFrameSync();
        return; // abort
    }

    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit(); // get dibit from symbol

    processVoiceDibit(dibit);

    if (m_symbolIndex == IN_DIBITS(DMR_TS_LEN) - 1) // last dibit
    {
        if (m_slot == DSDDMRSlot1)
        {
            m_voice1FrameCount++;

            if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN) // continuation expected on slot + 2
            {
                if (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 2
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRsyncOrSkip; // sync lookup or skip in slot 2
                    m_continuation = false; // TODO: true or false ?
                }
            }
            else // no super frame on going on this slot
            {
                m_dsdDecoder->m_voice1On = false;

                if (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 2
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->resetFrameSync(); // back to sync
                    m_continuation = false;
                }
            }
        }
        else if (m_slot == DSDDMRSlot2)
        {
            m_voice2FrameCount++;

            if (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN) // continuation expected on slot + 2
            {
                if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 1
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRsyncOrSkip; // sync lookup or skip in slot 1
                    m_continuation = false; // TODO: true or false ?
                }
            }
            else
            {
                m_dsdDecoder->m_voice2On = false;

                if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN)
                {
                    m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice; // voice continuation in slot 1
                    m_continuation = true;
                }
                else
                {
                    m_dsdDecoder->resetFrameSync(); // back to sync
                    m_continuation = false;
                }
            }
        }

        m_symbolIndex = 0;
    }
    else
    {
        m_symbolIndex++;
    }

    m_cachSymbolIndex++; // last dibit counts
}

void DSDDMR::processSyncOrSkip()
{
    const int sync_db_size = IN_DIBITS(DMR_SYNC_LEN);
    const DSDSync::SyncPattern patterns[2] = { DSDSync::SyncDMRDataBS, DSDSync::SyncDMRVoiceBS };

    if (m_symbolIndex > sync_db_size) // accumulate enough symbols to look for a sync
    {
        DSDSync syncEngine;
        syncEngine.matchSome(m_dsdDecoder->m_dsdSymbol.getSyncDibitBack(sync_db_size), sync_db_size, patterns, 2);

        if (syncEngine.isMatching(DSDSync::SyncDMRDataBS))
        {
//    DSD_LOG("DSDDMR::processSyncOrSkip: data sync");
            noteCSBKSyncAcquired();
            processDataFirstHalf(90);
            m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRdata;
            return;
        }
        else if (syncEngine.isMatching(DSDSync::SyncDMRVoiceBS))
        {
//    DSD_LOG("DSDDMR::processSyncOrSkip: voice sync");
            processVoiceFirstHalf(90);
            m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice;
            return;
        }
    }

    if (m_symbolIndex == IN_DIBITS(DMR_TS_LEN) - 1) // last dibit
    {
        // We crossed a full burst without seeing a valid sync pattern.
        noteCSBKSyncLost();

        // return to voice super frame
        m_slot = (DSDDMRSlot) (((int) m_slot + 1) % 2); // to keep the slot in the next slot period fake a slot reversal
        m_continuation = true;
        m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoice;
        m_symbolIndex = 0;
    }
    else
    {
        m_symbolIndex++;
    }

    m_cachSymbolIndex++; // last dibit counts
}

void DSDDMR::processVoiceMS()
{
    int dibit = m_dsdDecoder->m_dsdSymbol.getDibit(); // get dibit from symbol

    processVoiceDibit(dibit);

    if (m_symbolIndex == IN_DIBITS(DMR_TS_LEN) - 1) // last dibit
    {
        m_voice1FrameCount++;
//    DSD_LOG("DSDDMR::processVoiceMS: " << m_symbolIndex << " : " << m_voice1FrameCount);

        if (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN) // continuation expected on slot + 2
        {
            m_dsdDecoder->m_dsdSymbol.setNoSignal(true);
            m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRSkipMS; // skip next slot
        }
        else // no super frame on going on this slot
        {
            m_dsdDecoder->m_voice1On = false;
            m_dsdDecoder->resetFrameSync(); // back to sync
        }

        m_symbolIndex = 0;
    }
    else
    {
        m_symbolIndex++;
    }
}

void DSDDMR::processSkipMS()
{

    if (m_symbolIndex == IN_DIBITS(DMR_TS_LEN) - 1) // last dibit
    {
//    DSD_LOG("DSDDMR::processSkipMS: " << m_symbolIndex);
        // return to voice super frame
        m_dsdDecoder->m_dsdSymbol.setNoSignal(false);
        m_dsdDecoder->m_fsmState = DSDDecoder::DSDprocessDMRvoiceMS;
        m_symbolIndex = 0;
    }
    else
    {
        m_symbolIndex++;
    }
}

void DSDDMR::processDataFirstHalf(unsigned int shiftBack)
{
    unsigned char *dibit_p = m_dsdDecoder->m_dsdSymbol.getDibitBack(shiftBack);

//    DSD_LOG("DSDDMR::processDataFirstHalf");

    for (m_symbolIndex = 0; m_symbolIndex < 90; m_symbolIndex++, m_cachSymbolIndex++)
    {
        processDataDibit(dibit_p[m_symbolIndex]);
    }
}

void DSDDMR::processDataFirstHalfMS()
{
    unsigned char *dibit_p = m_dsdDecoder->m_dsdSymbol.getDibitBack(78+1);

//    DSD_LOG("DSDDMR::processDataFirstHalfMS");

    for (m_symbolIndex = 12; m_symbolIndex < 90; m_symbolIndex++, m_cachSymbolIndex++)
    {
        processDataDibit(dibit_p[m_symbolIndex]);
    }
}

void DSDDMR::processVoiceFirstHalf(unsigned int shiftBack)
{
    unsigned char *dibit_p = m_dsdDecoder->m_dsdSymbol.getDibitBack(shiftBack);

//    DSD_LOG("DSDDMR::processVoiceFirstHalf");

    for (m_symbolIndex = 0; m_symbolIndex < 90; m_symbolIndex++, m_cachSymbolIndex++)
    {
        processVoiceDibit(dibit_p[m_symbolIndex]);
    }

    if (m_slot == DSDDMRSlot1)
    {
        m_voice1FrameCount = 0;
        m_dsdDecoder->m_voice1On = true;
        m_voice1EmbSig_dibitsIndex = 0;
        m_voice1EmbSig_OK = true;
    }
    else if (m_slot == DSDDMRSlot2)
    {
        m_voice2FrameCount = 0;
        m_dsdDecoder->m_voice2On = true;
        m_voice2EmbSig_dibitsIndex = 0;
        m_voice2EmbSig_OK = true;
    }
    else // invalid
    {
        m_voice1FrameCount = DMR_VOX_SUPERFRAME_LEN;
        m_voice2FrameCount = DMR_VOX_SUPERFRAME_LEN;
        m_dsdDecoder->m_voice1On = false;
        m_dsdDecoder->m_voice2On = false;
        m_voice1EmbSig_OK = false;
        m_voice2EmbSig_OK = false;
    }

}

void DSDDMR::processVoiceFirstHalfMS()
{
    unsigned char *dibit_p = m_dsdDecoder->m_dsdSymbol.getDibitBack(78+1); // no CACH with MS

    for (m_symbolIndex = 12; m_symbolIndex < 90; m_symbolIndex++, m_cachSymbolIndex++)
    {
        processVoiceDibit(dibit_p[m_symbolIndex]);
    }

    // only one slot in MS
    m_slot = DSDDMRSlot1;
    memcpy(&m_dsdDecoder->m_state.slot0light[4], "VOX", 3);
    m_voice1FrameCount = 0;
    m_dsdDecoder->m_voice1On = true;
    m_voice1EmbSig_dibitsIndex = 0;
    m_voice1EmbSig_OK = true;
}

void DSDDMR::processDataDibit(unsigned char dibit)
{
    int nextPartOff = IN_DIBITS(DMR_CACH_LEN);

    // CACH

    if (m_symbolIndex < nextPartOff)
    {
        if (m_burstType == DSDDMRBaseStation)
        {
            m_cachBits[m_cachInterleave[2*m_symbolIndex]]   = (dibit >> 1) & 1;
            m_cachBits[m_cachInterleave[2*m_symbolIndex+1]] = dibit & 1;

            if(m_symbolIndex == nextPartOff-1)
            {
                decodeCACH(m_cachBits);

                //DSD_LOG("DSDDMR::processDataDibit: start frame: slot: " << (int) m_slot << " VC1: " << m_voice1FrameCount << " VC2: " << m_voice2FrameCount);
            }
        }
        return;
    }

    // data first half
    nextPartOff += IN_DIBITS(DMR_DATA_PART_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        m_dataDibits[m_symbolIndex - IN_DIBITS(DMR_CACH_LEN)] = dibit;
        return;
    }

    // Slot Type first half
    nextPartOff += IN_DIBITS(DMR_SLOT_TYPE_PART_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        m_slotTypePDU_dibits[m_symbolIndex - IN_DIBITS(DMR_CACH_LEN + DMR_DATA_PART_LEN)] = dibit;
        return;
    }

    // Sync or embedded signalling
    nextPartOff += IN_DIBITS(DMR_SYNC_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        // TODO
        return;
    }

    // Slot Type second half
    nextPartOff += IN_DIBITS(DMR_SLOT_TYPE_PART_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        int slotTypePDUOff = IN_DIBITS(DMR_SLOT_TYPE_PART_LEN) +
            m_symbolIndex - IN_DIBITS(DMR_CACH_LEN + DMR_DATA_PART_LEN +
                                      DMR_SLOT_TYPE_PART_LEN + DMR_SYNC_LEN);
        m_slotTypePDU_dibits[slotTypePDUOff] = dibit;

        if (m_symbolIndex == nextPartOff - 1)
        {
            processSlotTypePDU();
        }
        return;
    }

    // data second half
    nextPartOff += IN_DIBITS(DMR_DATA_PART_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        int dataSecIdx = IN_DIBITS(DMR_DATA_PART_LEN) + (m_symbolIndex - (nextPartOff - IN_DIBITS(DMR_DATA_PART_LEN)));
        m_dataDibits[dataSecIdx] = dibit;

        if (m_symbolIndex == nextPartOff - 1)
        {
            unsigned char infoBits[96];
            if (decodeBPTC196_96(infoBits))
            {
                switch (m_dataType)
                {
                    case DSDDMRDataCSBK:
                        decodeCSBK(infoBits);
                        break;
                    case DSDDMRDataMBCHeader:
                        decodeMBCHeader(infoBits);
                        break;
                    case DSDDMRDataMBCContinuation:
                        decodeMBCContinuation(infoBits);
                        break;
                    default:
                        break;
                }
            }
        }
        return;
    }
}

void DSDDMR::BasicPrivacyXOR(unsigned char *dibit, int index)
{
    if (m_dsdDecoder->m_opts.dmr_bp_key == 0) {
        return; // Basic Privacy not used
    }

    unsigned char key_number = m_dsdDecoder->m_opts.dmr_bp_key - 1;
    unsigned short key = BasicPrivacyKeys[key_number];
    const int key_bits = 16;

    if (index < 24) // 48 first bits (data)
    {
        int off = key_bits - ((index % 8) + 1) * 2;
        unsigned char out = *dibit ^ ((key >> off) & 3);
        *dibit = out;
    }
    else if (index == 24) // 49th bit (MSB) (data)
    {
        unsigned char msb = (*dibit >> 1) ^ (key >> 15);
        *dibit = (msb << 1) + (*dibit & 1);
    }
}

void DSDDMR::processVoiceDibit(unsigned char dibit)
{
    int nextPartOff = IN_DIBITS(DMR_CACH_LEN);
    int CurOff = 0;
    // CACH

    if (m_symbolIndex < nextPartOff)
    {
        if (m_burstType == DSDDMRBaseStation)
        {
            m_cachBits[m_cachInterleave[2*m_symbolIndex]]   = (dibit >> 1) & 1;
            m_cachBits[m_cachInterleave[2*m_symbolIndex+1]] = dibit & 1;

            if(m_symbolIndex == nextPartOff-1)
            {
                decodeCACH(m_cachBits);

                if (m_cachOK)
                {
                    if (m_slot == DSDDMRSlot1) {
                        memcpy(&m_dsdDecoder->m_state.slot0light[4], "VOX", 3);
                    } else if (m_slot == DSDDMRSlot2) {
                        memcpy(&m_dsdDecoder->m_state.slot1light[4], "VOX", 3);
                    }
                }

    //            DSD_LOG("DSDDMR::processVoiceDibit: start frame: slot: " << (int) m_slot << " VC1: " << m_voice1FrameCount << " VC2: " << m_voice2FrameCount);
            }
        }
        return;
    }

    // voice frame 1
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_VOCODER_FRAME_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        int mbeIndex = m_symbolIndex - IN_DIBITS(DMR_CACH_LEN);

        if (mbeIndex == 0)
        {
            w = rW;
            x = rX;
            y = rY;
            z = rZ;

            memset((void*)m_dsdDecoder->ambe_fr, 0, sizeof(m_dsdDecoder->ambe_fr));

            if (m_slot == DSDDMRSlot1) {
                memset((void *) m_dsdDecoder->m_mbeDVFrame1, 0, IN_BYTES(DMR_VOCODER_FRAME_LEN)); // initialize DVSI frame 1
            } else {
                memset((void *) m_dsdDecoder->m_mbeDVFrame2, 0, IN_BYTES(DMR_VOCODER_FRAME_LEN)); // initialize DVSI frame 2
            }
        }

        BasicPrivacyXOR(&dibit, mbeIndex);
		if (dibit > 3) {
			dibit = 0; // invalid dibit, should not happen, but to be safe
		}

        m_dsdDecoder->ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
        m_dsdDecoder->ambe_fr[*y][*z] = (1 & dibit);        // bit 0
        w++;
        x++;
        y++;
        z++;

        if (m_slot == DSDDMRSlot1) {
            storeSymbolDV(m_dsdDecoder->m_mbeDVFrame1, mbeIndex, dibit); // store dibit for DVSI hardware decoder
        } else { // it does not matter if CACH is undefined as it will be aborted later
            storeSymbolDV(m_dsdDecoder->m_mbeDVFrame2, mbeIndex, dibit); // store dibit for DVSI hardware decoder
        }

        if (mbeIndex == IN_DIBITS(DMR_VOCODER_FRAME_LEN) - 1)
        {
            if (m_slot == DSDDMRSlot1)
            {
                m_dsdDecoder->m_mbeDecoder1.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                m_dsdDecoder->m_mbeDVReady1 = true; // Indicate that a DVSI frame is available
            }
            else if (m_slot == DSDDMRSlot2)
            {
                m_dsdDecoder->m_mbeDecoder2.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                m_dsdDecoder->m_mbeDVReady2 = true; // Indicate that a DVSI frame is available
            }
        }
        return;
    }

    // voice frame 2 first half
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_VOCODER_FRAME_LEN / 2);
    if (m_symbolIndex < nextPartOff)
    {
        int mbeIndex = m_symbolIndex - CurOff;

        if (mbeIndex == 0)
        {
            w = rW;
            x = rX;
            y = rY;
            z = rZ;

            memset((void *) m_mbeDVFrame, 0, IN_BYTES(DMR_VOCODER_FRAME_LEN)); // initialize DVSI frame
        }

        BasicPrivacyXOR(&dibit, mbeIndex);

        m_dsdDecoder->ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
        m_dsdDecoder->ambe_fr[*y][*z] = (1 & dibit);        // bit 0
        w++;
        x++;
        y++;
        z++;

        storeSymbolDV(m_mbeDVFrame, mbeIndex, dibit); // store dibit for DVSI hardware decoder
        return;
    }

    // EMB first half
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_EMB_PART_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        m_emb_dibits[m_symbolIndex - CurOff] = dibit;
        return;
    }

    // Embedded signaling
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_ES_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        m_voiceEmbSig_dibits[m_symbolIndex - CurOff] = dibit;
        return;
    }

    // EMB second half
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_EMB_PART_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        m_emb_dibits[m_symbolIndex + IN_DIBITS(DMR_EMB_PART_LEN) - CurOff] = dibit;

        if (m_symbolIndex == nextPartOff - 1)
        {
            if ((m_slot == DSDDMRSlot1) && (m_voice1FrameCount > 0) && (m_voice1FrameCount < DMR_VOX_SUPERFRAME_LEN))
            {
                if (processEMB())
                {
                    if (processVoiceEmbeddedSignalling(m_voice1EmbSig_dibitsIndex, m_voice1EmbSigRawBits, m_voice1EmbSig_OK, m_slot1Addresses))
                    {
                        textVoiceEmbeddedSignalling(m_slot1Addresses, m_dsdDecoder->m_state.slot0light);
//                        DSD_LOG("DSDDMR::processVoiceDibit: source: " << m_slot1Addresses.m_source << " target: " << m_slot1Addresses.m_target << " group: " << m_slot1Addresses.m_group);
                    }
                }
            }
            else if ((m_slot == DSDDMRSlot2) && (m_voice2FrameCount > 0) && (m_voice2FrameCount < DMR_VOX_SUPERFRAME_LEN))
            {
                if (processEMB())
                {
                    if (processVoiceEmbeddedSignalling(m_voice2EmbSig_dibitsIndex, m_voice2EmbSigRawBits, m_voice2EmbSig_OK, m_slot2Addresses))
                    {
                        textVoiceEmbeddedSignalling(m_slot2Addresses, m_dsdDecoder->m_state.slot1light);
//                        DSD_LOG("DSDDMR::processVoiceDibit: source: " << m_slot2Addresses.m_source << " target: " << m_slot2Addresses.m_target << " group: " << m_slot2Addresses.m_group);
                    }
                }
            }
        }
        return;
    }

    // voice frame 2 second half
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_VOCODER_FRAME_LEN / 2);
    if (m_symbolIndex < nextPartOff)
    {
        int mbeIndex = m_symbolIndex - (CurOff - IN_DIBITS(DMR_VOCODER_FRAME_LEN / 2));

        BasicPrivacyXOR(&dibit, mbeIndex);

        m_dsdDecoder->ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
        m_dsdDecoder->ambe_fr[*y][*z] = (1 & dibit);        // bit 0
        w++;
        x++;
        y++;
        z++;

        storeSymbolDV(m_mbeDVFrame, mbeIndex, dibit); // store dibit for DVSI hardware decoder

        if (mbeIndex == IN_DIBITS(DMR_VOCODER_FRAME_LEN) - 1)
        {
            if (m_slot == DSDDMRSlot1)
            {
                m_dsdDecoder->m_mbeDecoder1.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                memcpy(m_dsdDecoder->m_mbeDVFrame1, m_mbeDVFrame, IN_BYTES(DMR_VOCODER_FRAME_LEN));
                m_dsdDecoder->m_mbeDVReady1 = true; // Indicate that a DVSI frame is available
            }
            else if (m_slot == DSDDMRSlot2)
            {
                m_dsdDecoder->m_mbeDecoder2.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                memcpy(m_dsdDecoder->m_mbeDVFrame2, m_mbeDVFrame, IN_BYTES(DMR_VOCODER_FRAME_LEN));
                m_dsdDecoder->m_mbeDVReady2 = true; // Indicate that a DVSI frame is available
            }
        }
        return;
    }

    // voice frame 3
    CurOff = nextPartOff;
    nextPartOff += IN_DIBITS(DMR_VOCODER_FRAME_LEN);
    if (m_symbolIndex < nextPartOff)
    {
        int mbeIndex = m_symbolIndex - CurOff;

        BasicPrivacyXOR(&dibit, mbeIndex);

        if (mbeIndex == 0)
        {
            w = rW;
            x = rX;
            y = rY;
            z = rZ;

            if (m_slot == DSDDMRSlot1) {
                memset((void *) m_dsdDecoder->m_mbeDVFrame1, 0, IN_BYTES(DMR_VOCODER_FRAME_LEN)); // initialize DVSI frame 1
            } else {
                memset((void *) m_dsdDecoder->m_mbeDVFrame2, 0, IN_BYTES(DMR_VOCODER_FRAME_LEN)); // initialize DVSI frame 2
            }
        }

        m_dsdDecoder->ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
        m_dsdDecoder->ambe_fr[*y][*z] = (1 & dibit);        // bit 0
        w++;
        x++;
        y++;
        z++;

        if (m_slot == DSDDMRSlot1) {
            storeSymbolDV(m_dsdDecoder->m_mbeDVFrame1, mbeIndex, dibit); // store dibit for DVSI hardware decoder
        } else { // it does not matter if CACH is undefined as it will be aborted later
            storeSymbolDV(m_dsdDecoder->m_mbeDVFrame2, mbeIndex, dibit); // store dibit for DVSI hardware decoder
        }

        if (mbeIndex == IN_DIBITS(DMR_VOCODER_FRAME_LEN) - 1)
        {
            if (m_slot == DSDDMRSlot1)
            {
                m_dsdDecoder->m_mbeDecoder1.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                m_dsdDecoder->m_mbeDVReady1 = true; // Indicate that a DVSI frame is available
            }
            else if (m_slot == DSDDMRSlot2)
            {
                m_dsdDecoder->m_mbeDecoder2.processFrame(0, m_dsdDecoder->ambe_fr, 0);
                m_dsdDecoder->m_mbeDVReady2 = true; // Indicate that a DVSI frame is available
            }
        }
        return;
    }
}

void DSDDMR::decodeCACH(unsigned char *cachBits)
{
    m_cachOK = true;

    if (m_continuation)
    {
        m_slot = (DSDDMRSlot) (((int) m_slot + 1) % 2);
//        DSD_LOG("DSDDMR::decodeCACH: CC: at: " << m_cachSymbolIndex << " slot: " << (int) m_slot);
        m_continuation = false;
        m_cachSymbolIndex = 0; // restart counting
    }
    else
    {
        // Hamming (7,4) decode and store results if successful
        if (m_hamming_7_4.decode(cachBits)) // positive CACH information
        {
            unsigned int slotIndex = cachBits[1] & 1;
            m_dsdDecoder->m_state.currentslot = slotIndex; // FIXME: remove this when done with new voice processing

            if (slotIndex)
            {
                m_slotText = m_dsdDecoder->m_state.slot1light;
                m_dsdDecoder->m_state.slot0light[0] = ((cachBits[0] & 1) ? '*' : '.'); // the activity indicator is shifted by one slot
            }
            else
            {
                m_slotText = m_dsdDecoder->m_state.slot0light;
                m_dsdDecoder->m_state.slot1light[0] = ((cachBits[0] & 1) ? '*' : '.'); // the activity indicator is shifted by one slot
            }

            m_slot = (DSDDMRSlot) slotIndex;
            m_lcss = 2*cachBits[2] + cachBits[3];

//            DSD_LOG("DSDDMR::decodeCACH: OK: at: " << m_cachSymbolIndex << " Slot: " << (int) cachBits[1] << " LCSS: " << (int) m_lcss);

            m_cachSymbolIndex = 0; // restart counting
        }
        else
        {
            m_slot = DSDDMRSlotUndefined;
            m_cachOK = false;
//            DSD_LOG("DSDDMR::decodeCACH: KO: at: " << m_cachSymbolIndex);
        }
    }
}

void DSDDMR::processSlotTypePDU()
{
    unsigned char slotTypeBits[DMR_SLOT_TYPE_PART_LEN * 2];

    for (int i = 0; i < DMR_SLOT_TYPE_PART_LEN; i++)
    {
        slotTypeBits[2*i]     = (m_slotTypePDU_dibits[i] >> 1) & 1;
        slotTypeBits[2*i + 1] = m_slotTypePDU_dibits[i] & 1;
    }

    if (m_golay_20_8.decode(slotTypeBits))
    {
        m_colorCode = (slotTypeBits[0] << 3) + (slotTypeBits[1] << 2) + (slotTypeBits[2] << 1) + slotTypeBits[3];
        sprintf(&m_slotText[1], "%02d ", m_colorCode);

        unsigned int dataType = (slotTypeBits[4] << 3) + (slotTypeBits[5] << 2) + (slotTypeBits[6] << 1) + slotTypeBits[7];

        if (dataType >= DMR_TYPES_COUNT)
        {
            m_dataType = DSDDMRDataReserved;
            memcpy(&m_slotText[4], "RES", 3);
        }
        else
        {
            m_dataType = (DSDDMRDataTYpe) dataType;
            memcpy(&m_slotText[4], m_slotTypeText[dataType], 3);
        }

        if (m_verbosity > 2) DSD_LOG("DT: " << m_slotText);
    }
    else
    {
        memcpy(&m_slotText[1], "-- UNK", 6);
        if (m_verbosity > 1) DSD_LOG("DSDDMR::processSlotTypePDU KO");
    }
}

bool DSDDMR::processEMB()
{
    unsigned char embBits[DMR_EMB_PART_LEN * 2];

    for (int i = 0; i < DMR_EMB_PART_LEN; i++)
    {
        embBits[2*i]     = (m_emb_dibits[i] >> 1) & 1;
        embBits[2*i + 1] = m_emb_dibits[i] & 1;
    }

    if (m_qr_16_7_6.decode(embBits))
    {
        m_colorCode = (embBits[0] << 3) + (embBits[1] << 2) + (embBits[2] << 1) + embBits[3];
        sprintf(&m_slotText[1], "%02d", m_colorCode);
        m_slotText[3] = ' ';
        m_lcss = (embBits[5] << 1) + embBits[6];
        return true;
    }
    else
    {
        return false;
    }
}

bool DSDDMR::processVoiceEmbeddedSignalling(int& voiceEmbSig_dibitsIndex,
        unsigned char *voiceEmbSigRawBits,
        bool& voiceEmbSig_OK,
        DMRAddresses& addresses)
{
    if (m_lcss != SingleLC_FirstCSBK) // skip RC
    {
        unsigned char parityCheck = 0;

        for (int i = 0; i < IN_DIBITS(DMR_ES_LEN); i++)
        {
            if (voiceEmbSig_dibitsIndex > 63) { // prevent segfault
                break;
            }

            int bit1Index = m_embSigInterleave[2*voiceEmbSig_dibitsIndex];
            int bit0Index = m_embSigInterleave[2*voiceEmbSig_dibitsIndex + 1];

            if ((i%4) == 0)
            {
                parityCheck = 0;
            }

            voiceEmbSigRawBits[bit1Index] = (1 & (m_voiceEmbSig_dibits[i] >> 1)); // bit 1
            voiceEmbSigRawBits[bit0Index] = (1 & m_voiceEmbSig_dibits[i]);        // bit 0
            parityCheck ^= voiceEmbSigRawBits[bit1Index];
            parityCheck ^= voiceEmbSigRawBits[bit0Index];

            if ((i%4) == 3)
            {
                if (parityCheck != 0)
                {
                    voiceEmbSig_OK = false;
                    break;
                }
            }

            voiceEmbSig_dibitsIndex++;
        }

        if (voiceEmbSig_dibitsIndex == 16*4) // BPTC matrix collected
        {
            if (m_hamming_16_11_4.decode(voiceEmbSigRawBits, 0, 7)) // TODO: 5 bit checksum
            {
                unsigned char flco = (voiceEmbSigRawBits[2] << 5)
                        + (voiceEmbSigRawBits[3] << 4)
                        + (voiceEmbSigRawBits[4] << 3)
                        + (voiceEmbSigRawBits[5] << 2)
                        + (voiceEmbSigRawBits[6] << 1)
                        + (voiceEmbSigRawBits[7]);

                addresses.m_group = (flco == 0);

                addresses.m_target = (voiceEmbSigRawBits[16*2 + 2] << 23) // (LC47)
                        + (voiceEmbSigRawBits[16*2 + 3] << 22)
                        + (voiceEmbSigRawBits[16*2 + 4] << 21)
                        + (voiceEmbSigRawBits[16*2 + 5] << 20)
                        + (voiceEmbSigRawBits[16*2 + 6] << 19)
                        + (voiceEmbSigRawBits[16*2 + 7] << 18)
                        + (voiceEmbSigRawBits[16*2 + 8] << 17)
                        + (voiceEmbSigRawBits[16*2 + 9] << 16) // 2:7
                        + (voiceEmbSigRawBits[16*3 + 0] << 15) // (LC39)
                        + (voiceEmbSigRawBits[16*3 + 1] << 14)
                        + (voiceEmbSigRawBits[16*3 + 2] << 13)
                        + (voiceEmbSigRawBits[16*3 + 3] << 12)
                        + (voiceEmbSigRawBits[16*3 + 4] << 11)
                        + (voiceEmbSigRawBits[16*3 + 5] << 10)
                        + (voiceEmbSigRawBits[16*3 + 6] << 9)
                        + (voiceEmbSigRawBits[16*3 + 7] << 8)  // 1:7
                        + (voiceEmbSigRawBits[16*3 + 8] << 7)  // (LC31)
                        + (voiceEmbSigRawBits[16*3 + 9] << 6)
                        + (voiceEmbSigRawBits[16*4 + 0] << 5)  // (LC29)
                        + (voiceEmbSigRawBits[16*4 + 1] << 4)
                        + (voiceEmbSigRawBits[16*4 + 2] << 3)
                        + (voiceEmbSigRawBits[16*4 + 3] << 2)
                        + (voiceEmbSigRawBits[16*4 + 4] << 1)
                        + (voiceEmbSigRawBits[16*4 + 5]);      // (LC24)
                addresses.m_source = (voiceEmbSigRawBits[16*4 + 6] << 23) // (LC23)
                        + (voiceEmbSigRawBits[16*4 + 7] << 22)
                        + (voiceEmbSigRawBits[16*4 + 8] << 21)
                        + (voiceEmbSigRawBits[16*4 + 9] << 20)
                        + (voiceEmbSigRawBits[16*5 + 0] << 19)
                        + (voiceEmbSigRawBits[16*5 + 1] << 18)
                        + (voiceEmbSigRawBits[16*5 + 2] << 17)
                        + (voiceEmbSigRawBits[16*5 + 3] << 16) // 2:7
                        + (voiceEmbSigRawBits[16*5 + 4] << 15) // (LC15)
                        + (voiceEmbSigRawBits[16*5 + 5] << 14)
                        + (voiceEmbSigRawBits[16*5 + 6] << 13)
                        + (voiceEmbSigRawBits[16*5 + 7] << 12)
                        + (voiceEmbSigRawBits[16*5 + 8] << 11)
                        + (voiceEmbSigRawBits[16*5 + 9] << 10)
                        + (voiceEmbSigRawBits[16*6 + 0] << 9)
                        + (voiceEmbSigRawBits[16*6 + 1] << 8)  // 1:7
                        + (voiceEmbSigRawBits[16*6 + 2] << 7)  // (LC7)
                        + (voiceEmbSigRawBits[16*6 + 3] << 6)
                        + (voiceEmbSigRawBits[16*6 + 4] << 5)
                        + (voiceEmbSigRawBits[16*6 + 5] << 4)
                        + (voiceEmbSigRawBits[16*6 + 6] << 3)
                        + (voiceEmbSigRawBits[16*6 + 7] << 2)
                        + (voiceEmbSigRawBits[16*6 + 8] << 1)
                        + (voiceEmbSigRawBits[16*6 + 9]);      // (LC0)

                return true; // we have a result
            }
            else
            {
                DSD_LOG("DSDDMR::processVoiceEmbeddedSignalling: decode error");
                voiceEmbSig_OK = false;
            }
        }
    }

    return false; // no result yet or KO
}

void DSDDMR::storeSymbolDV(unsigned char *mbeFrame, int dibitindex, unsigned char dibit, bool invertDibit)
{
    if (m_dsdDecoder->m_mbelibEnable)
    {
        return;
    }

    if (invertDibit)
    {
        dibit = DSDcc::DSDSymbol::invert_dibit(dibit);
    }

    mbeFrame[dibitindex/4] |= (dibit << (6 - 2*(dibitindex % 4)));
}

void DSDDMR::textVoiceEmbeddedSignalling(DMRAddresses& addresses, char *slotText)
{
    sprintf(&slotText[8],  "%08u", addresses.m_source);
    sprintf(&slotText[18], "%08u", addresses.m_target);
    slotText[16] = '>';

    if (addresses.m_group) {
        slotText[17] = 'G';
    } else {
        slotText[17] = 'U';
    }
}

const char *DSDDMR::getSlot0Text() const
{
    return m_dsdDecoder->m_state.slot0light;
}

const char *DSDDMR::getSlot1Text() const
{
    return m_dsdDecoder->m_state.slot1light;
}

unsigned char DSDDMR::getColorCode() const
{
    return m_dsdDecoder->m_state.ccnum;
}

// ========================================================================================
// BPTC(196,96) decoder – ETSI TS 102 361-1 §B.1
// ========================================================================================

// Hamming(13,9,3) single-bit error correction – adapted from MMDVM CHamming::decode1393
// Used for BPTC(196,96) column correction.
static bool hamming1393(unsigned char *d)
{
    unsigned char c0 = d[0] ^ d[1] ^ d[3] ^ d[5] ^ d[6];
    unsigned char c1 = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7];
    unsigned char c2 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    unsigned char c3 = d[0] ^ d[2] ^ d[4] ^ d[5] ^ d[8];

    unsigned char n = 0;
    if (c0 != d[9])  n |= 0x01U;
    if (c1 != d[10]) n |= 0x02U;
    if (c2 != d[11]) n |= 0x04U;
    if (c3 != d[12]) n |= 0x08U;

    switch (n) {
        case 0x01U: d[9]  ^= 1U; return true;
        case 0x02U: d[10] ^= 1U; return true;
        case 0x04U: d[11] ^= 1U; return true;
        case 0x08U: d[12] ^= 1U; return true;
        case 0x0FU: d[0]  ^= 1U; return true;
        case 0x07U: d[1]  ^= 1U; return true;
        case 0x0EU: d[2]  ^= 1U; return true;
        case 0x05U: d[3]  ^= 1U; return true;
        case 0x0AU: d[4]  ^= 1U; return true;
        case 0x0DU: d[5]  ^= 1U; return true;
        case 0x03U: d[6]  ^= 1U; return true;
        case 0x06U: d[7]  ^= 1U; return true;
        case 0x0CU: d[8]  ^= 1U; return true;
        default:    return false;
    }
}

bool DSDDMR::decodeBPTC196_96(unsigned char *infoBits)
{
    // Step 1: unpack 98 stored dibits to 196 channel bits
    unsigned char channelBits[196];
    for (int i = 0; i < 98; i++)
    {
        channelBits[2*i]     = (m_dataDibits[i] >> 1) & 1;
        channelBits[2*i + 1] =  m_dataDibits[i] & 1;
    }

    // Step 2: deinterleave – gather: deInterData[a] = channelBits[(a*181)%196]
    // Per ETSI TS 102 361-1 §B.1 and MMDVM reference implementation.
    unsigned char deInterData[196] = {};
    for (unsigned int a = 0; a < 196; a++)
        deInterData[a] = channelBits[(a * 181U) % 196U];

    // Matrix layout (ETSI BPTC(196,96), 13 rows × 15 cols + 1 R bit = 196 bits):
    //   deInterData[0]          = R(3) padding bit (ignored)
    //   deInterData[1..195]     = 13 rows × 15 cols
    //     Rows 0-8  (data rows): each is a Hamming(15,11,3) codeword
    //       row 0 cols 0-2 = R bits; cols 3-10 = data; cols 11-14 = Hamming parity
    //       rows 1-8 cols 0-10 = data; cols 11-14 = Hamming parity
    //     Rows 9-12 (col parity rows): 4 parity bits for each column's Hamming(13,9,3)
    //   Column c has 13 values: deInterData[c+1 + 15*a] for a=0..12
    //     first 9 values (a=0..8) are data, last 4 (a=9..12) are Hamming(13,9) parity

    // Step 3: iterative column + row Hamming correction (MMDVM-style, up to 5 passes).
    //
    // Column correction (Hamming(13,9,3)) handles rows with multiple bit errors that
    // row-only correction cannot fix (e.g. 2 errors split across different columns).
    // After column correction reduces multi-bit row errors to single-bit errors, the
    // row Hamming(15,11,3) pass finishes the job.
    //
    // Safety: rows 7 and 8 (the CRC bytes, deInter positions c+1+15*7 and c+1+15*8)
    // are excluded from column write-back. If the column parity rows (9-12) are noisy,
    // a false column correction could corrupt the stored CRC; row-only correction is
    // reliable enough for those two rows and this guard costs nothing on clean signals.
    bool fixing;
    unsigned int pass = 0;
    do {
        fixing = false;

        // Column Hamming(13,9,3) pass – 15 columns
        for (unsigned int c = 0; c < 15; c++)
        {
            unsigned char col[13];
            for (unsigned int r = 0; r < 13; r++)
                col[r] = deInterData[c + 1 + 15 * r];

            if (hamming1393(col))
            {
                for (unsigned int r = 0; r < 13; r++)
                {
                    //if (r == 7 || r == 8) continue; // guard CRC bytes
                    unsigned int idx = c + 1 + 15 * r;
                    if (deInterData[idx] != col[r])
                    {
                        deInterData[idx] = col[r];
                        fixing = true;
                    }
                }
            }
        }

        // Row Hamming(15,11,3) pass – 9 data rows
        for (int r = 0; r < 9; r++)
        {
            if (m_hamming_15_11.decode(&deInterData[1 + r * 15], nullptr, 1))
                fixing = true;
        }

        pass++;
    } while (fixing && pass < 5);

    // Step 4: extract 96 data bits from the corrected deinterleaved matrix
    //   row 0, cols 3-10 → deInterData[4..11]        (8 bits; R bits at cols 0-2)
    //   rows 1-8, cols 0-10 → deInterData[1+r*15 .. 11+r*15]  (11 bits each)
    int pos = 0;
    for (int a = 4; a <= 11; a++)
        infoBits[pos++] = deInterData[a];
    for (int r = 1; r <= 8; r++)
        for (int a = 0; a <= 10; a++)
            infoBits[pos++] = deInterData[1 + r * 15 + a];
    // pos == 96

    return true;
}

// ========================================================================================
// CSBK / MBC decoders
// ========================================================================================

static const char *csbkoName(unsigned char csbko)
{
    // Tier III aliases from ETSI TS 102 361-4 v1.12.1 (tables 7.1-7.4)
    // plus a few legacy labels kept for compatibility with existing logs.
    switch (csbko)
    {
        case 0x00: return "BS OutAct ";
        case 0x01: return "UU VoReq  ";
        case 0x02: return "UU VoAns  ";
        case 0x03: return "NACK      ";
        case 0x04: return "UU D-Grant";
        case 0x05: return "UU D-ChanG";
        case 0x06: return "RChk Req  ";
        case 0x07: return "RChk Rsp  ";
        case 0x08: return "CallAlert ";
        case 0x09: return "CallAltNAK";
        case 0x0A: return "CallAlert ";
        case 0x0B: return "GV VoReq  ";
        case 0x0C: return "GV VoAns  ";
        case 0x0D: return "GV VoTerm ";
        case 0x0E: return "GV VoCont ";
        case 0x14: return "UU VoCGrt ";
        case 0x15: return "UU VoCGUpd";
        case 0x18: return "GV VoCGrt ";
        case 0x19: return "C_ALOHA   ";
        case 0x1A: return "GV CGrtExp";
        case 0x1C: return "C_AHOY    ";
        case 0x1D: return "AnnUpBS   ";
        case 0x1E: return "C_ACKVIT  ";
        case 0x1F: return "C_RAND    ";
        case 0x20: return "C_ACKD    ";
        case 0x21: return "Preamble  ";
        case 0x22: return "P_ACKD    ";
        case 0x23: return "P_ACKU    ";
        case 0x28: return "C_BCAST   ";
        case 0x2A: return "P_MAINT   ";
        case 0x2E: return "P_CLEAR   ";
        case 0x2F: return "P_PROTECT ";
        case 0x30: return "PV_GRANT  ";
        case 0x31: return "TV_GRANT  ";
        case 0x32: return "BTV_GRANT ";
        case 0x33: return "PD_GRANT  ";
        case 0x34: return "TD_GRANT  ";
        case 0x35: return "PV_GRANTDX";
        case 0x36: return "PD_GRANTDX";
        case 0x39: return "C_MOVE    ";
        default:   return nullptr;
    }
}

static uint16_t crcCCITT16(const unsigned char *bytes, int len, uint16_t init)
{
    uint16_t crc = init;
    for (int i = 0; i < len; i++)
    {
        crc ^= (uint16_t)bytes[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    }
    return crc;
}

static bool csbkCRCOK(const unsigned char *infoBits, uint16_t mask, int verbosity)

{
    // Pack all 96 bits into 12 bytes MSB-first
    unsigned char bytes[12] = {0};
    for (int i = 0; i < 96; i++)
        bytes[i / 8] |= (infoBits[i] << (7 - (i % 8)));

    // CRC-CCITT-16 over bytes[0..9], then ETSI post-CRC mask (TS 102 361-1 §B.3.12).
    uint16_t raw = crcCCITT16(bytes, 10, 0x0000);
    uint16_t computed = raw ^ 0xFFFF ^ mask;

    uint16_t received  = ((uint16_t)bytes[10] << 8) | bytes[11];
    
    if (computed != received)
    {
        uint16_t inferredMask = raw ^ 0xFFFF ^ received;

        if (verbosity > 0)
        {
            DSD_LOG("CRC FAIL: received=0x" << std::hex << std::setw(4) << received
                << " computed=0x" << std::setw(4) << computed
                //<< " raw=0x" << std::setw(4) << raw
                << " mask=0x" << std::setw(4) << mask
                << " inferredMask=0x" << std::setw(4) << inferredMask
                << std::dec);
        }
    }

    return computed == received;
}

static unsigned int bitsToUint(const unsigned char *bits, int nbBits)
{
    unsigned int v = 0;
    for (int i = 0; i < nbBits; i++) v = (v << 1) | bits[i];
    return v;
}

void DSDDMR::noteCSBKSyncAcquired()
{
    // Edge-triggered acquisition: only bump epoch when transitioning from unlocked to locked.
    if (m_csbkSyncLocked)
        return;

    m_csbkSyncLocked = true;
    if (m_csbkSyncEpoch == 0xFFFFFFFFU)
    {
        m_csbkSyncEpoch = 0;
        m_csbkLogStates.clear();
    }
    m_csbkSyncEpoch++;
}

void DSDDMR::noteCSBKSyncLost()
{
    m_csbkSyncLocked = false;
}

bool DSDDMR::shouldLogCSBK(unsigned char csbko, unsigned char mfid, bool crcOK, const std::string& messageText)
{
    // Highest verbosity: always print all CSBKs. Lowest: print CRC failures only.
    if (m_verbosity >= 3)
        return true;
    if (m_verbosity <= 0)
        return !crcOK;

    const auto now = std::chrono::steady_clock::now();
    const std::uint64_t nowMs = (std::uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    const std::uint32_t slotId = (m_slot == DSDDMRSlot2) ? 1U : 0U;
    const std::uint32_t key = (slotId << 16) | ((std::uint32_t) mfid << 8) | (std::uint32_t) csbko;
    const std::size_t signature = std::hash<std::string>{}(messageText);

    CSBKLogState& st = m_csbkLogStates[key];
    const bool firstAfterSync = (!st.seen) || (st.syncEpoch != m_csbkSyncEpoch);
    const bool changed = (!st.seen) || (st.signature != signature);
    const std::uint64_t heartbeatMs = (m_verbosity >= 2) ? 3000U : 10000U;
    const bool heartbeatDue = (!st.seen) || ((nowMs - st.lastLogMs) >= heartbeatMs);

    const bool log = (!crcOK) || firstAfterSync || changed || heartbeatDue;

    if (log)
    {
        st.signature = signature;
        st.lastLogMs = nowMs;
        st.syncEpoch = m_csbkSyncEpoch;
        st.seen = true;
    }

    return log;
}

void DSDDMR::decodeCSBK(const unsigned char *infoBits)
{
    unsigned char lb    = infoBits[0];
    unsigned char csbko = (unsigned char) bitsToUint(&infoBits[2], 6);
    unsigned char mfid  = (unsigned char) bitsToUint(&infoBits[8], 8);

    // Verify CRC-CCITT-16 with TS 102 361-1 §B.3.12 CSBK mask.
    bool crcOK = csbkCRCOK(infoBits, 0xA5A5, m_verbosity);

    const char *name = nullptr;
    if (mfid == 0x00) name = csbkoName(csbko);

    std::ostringstream msg;
    msg << "CSBK["
        << (m_slot == DSDDMRSlot1 ? "1" : "2")
        << "] "
        << (name ? name : "Unknown   ")
        << " CSBKO=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)csbko
        << " MFId=0x"  << std::setw(2) << (int)mfid
        << std::dec
        << (crcOK ? "" : " CRC-FAIL");

    if (mfid == 0x00) // ETSI standard opcodes
    {
        if (csbko == 0x19) // C_ALOHA (TS 102 361-4 v1.12.1 Table 7.19)
        {
            unsigned int version          = bitsToUint(&infoBits[19], 3);
            unsigned int tsccas           = bitsToUint(&infoBits[17], 1);
            unsigned int siteSync         = bitsToUint(&infoBits[18], 1);
            unsigned int offset           = bitsToUint(&infoBits[22], 1);
            unsigned int activeConnection = bitsToUint(&infoBits[23], 1);
            unsigned int mask             = bitsToUint(&infoBits[24], 5);
            unsigned int serviceFunction  = bitsToUint(&infoBits[29], 2);
            unsigned int nrandWait        = bitsToUint(&infoBits[31], 4);
            unsigned int reg              = bitsToUint(&infoBits[35], 1);
            unsigned int backoff          = bitsToUint(&infoBits[36], 4);
            unsigned int sic              = bitsToUint(&infoBits[40], 16);
            unsigned int msAddress        = bitsToUint(&infoBits[56], 24);

            msg << " Ver=" << version
                << " TSCCAS=" << tsccas
                << " Sync=" << siteSync
                << " Offs=" << offset
                << " Net=" << activeConnection
                << " Mask=" << mask
                << " SF=" << serviceFunction
                << " NW=" << nrandWait
                << " Reg=" << reg
                << " Backoff=" << backoff
                << " SIC=0x" << std::hex << std::setw(4) << std::setfill('0') << sic << std::dec
                << " MS=" << msAddress;
        }
        else if (csbko == 0x1C) // C_AHOY (Table 7.22)
        {
            unsigned int serviceOptionsMirror = bitsToUint(&infoBits[16], 7);
            unsigned int serviceKindFlag      = bitsToUint(&infoBits[23], 1);
            unsigned int als                  = bitsToUint(&infoBits[24], 1);
            unsigned int groupFlag            = bitsToUint(&infoBits[25], 1);
            unsigned int appendedBlocks       = bitsToUint(&infoBits[26], 2);
            unsigned int serviceKind          = bitsToUint(&infoBits[28], 4);
            unsigned int dst                  = bitsToUint(&infoBits[32], 24);
            unsigned int src                  = bitsToUint(&infoBits[56], 24);

            msg << " SOm=" << serviceOptionsMirror
                << " SKF=" << serviceKindFlag
                << " ALS=" << als
                << " GI=" << groupFlag
                << " App=" << appendedBlocks
                << " Kind=" << serviceKind
                << " Src=" << src
                << " Dst=" << dst;
        }
        else if (csbko == 0x20) // C_ACKD (Table 7.23)
        {
            unsigned int responseInfo = bitsToUint(&infoBits[16], 7);
            unsigned int reasonCode   = bitsToUint(&infoBits[23], 8);
            unsigned int reserved     = bitsToUint(&infoBits[31], 1);
            unsigned int target       = bitsToUint(&infoBits[32], 24);
            unsigned int addInfo      = bitsToUint(&infoBits[56], 24);

            msg << " RspInfo=" << responseInfo
                << " Reason=0x" << std::hex << std::setw(2) << std::setfill('0') << reasonCode << std::dec
                << " Rsv=" << reserved
                << " Tgt=" << target
                << " AddInfo=" << addInfo;
        }
        else if (csbko == 0x28) // C_BCAST (Table 7.20)
        {
            unsigned int annType = bitsToUint(&infoBits[16], 5);
            unsigned int parms1  = bitsToUint(&infoBits[21], 14);
            unsigned int reg     = bitsToUint(&infoBits[35], 1);
            unsigned int backoff = bitsToUint(&infoBits[36], 4);
            unsigned int sic     = bitsToUint(&infoBits[40], 16);
            unsigned int parms2  = bitsToUint(&infoBits[56], 24);

            msg << " AnnType=" << annType
                << " P1=" << parms1
                << " Reg=" << reg
                << " Backoff=" << backoff
                << " SIC=0x" << std::hex << std::setw(4) << std::setfill('0') << sic << std::dec
                << " P2=0x" << std::hex << std::setw(6) << std::setfill('0') << parms2 << std::dec;
        }
        else if (csbko == 0x2E) // P_CLEAR (Table 7.30)
        {
            unsigned int physChan = bitsToUint(&infoBits[16], 12);
            unsigned int gi       = bitsToUint(&infoBits[31], 1);
            unsigned int dst      = bitsToUint(&infoBits[32], 24);
            unsigned int src      = bitsToUint(&infoBits[56], 24);

            msg << " Ch=" << physChan
                << " GI=" << gi
                << " Src=" << src
                << " Dst=" << dst;
        }
        else if (csbko == 0x2F) // P_PROTECT (Table 7.31)
        {
            unsigned int protectKind = bitsToUint(&infoBits[28], 3);
            unsigned int gi          = bitsToUint(&infoBits[31], 1);
            unsigned int dst         = bitsToUint(&infoBits[32], 24);
            unsigned int src         = bitsToUint(&infoBits[56], 24);

            msg << " Kind=" << protectKind
                << " GI=" << gi
                << " Src=" << src
                << " Dst=" << dst;
        }
        else if (csbko == 0x31) // TV_GRANT (Table 7.11)
        {
            unsigned int physChan = bitsToUint(&infoBits[16], 12);
            unsigned int tdmaSlot = bitsToUint(&infoBits[28], 1);
            unsigned int late     = bitsToUint(&infoBits[29], 1);
            unsigned int emerg    = bitsToUint(&infoBits[30], 1);
            unsigned int offset   = bitsToUint(&infoBits[31], 1);
            unsigned int dst      = bitsToUint(&infoBits[32], 24);
            unsigned int src      = bitsToUint(&infoBits[56], 24);

            msg << " Ch=" << physChan
                << " TS=" << tdmaSlot
                << " Late=" << late
                << " Emerg=" << emerg
                << " Offs=" << offset
                << " Src=" << src
                << " Dst=" << dst;
        }
        else if (csbko == 0x34) // TD_GRANT (Table 7.15)
        {
            unsigned int physChan = bitsToUint(&infoBits[16], 12);
            unsigned int tdmaSlot = bitsToUint(&infoBits[28], 1);
            unsigned int hiRate   = bitsToUint(&infoBits[29], 1);
            unsigned int emerg    = bitsToUint(&infoBits[30], 1);
            unsigned int offset   = bitsToUint(&infoBits[31], 1);
            unsigned int dst      = bitsToUint(&infoBits[32], 24);
            unsigned int src      = bitsToUint(&infoBits[56], 24);

            msg << " Ch=" << physChan
                << " TS=" << tdmaSlot
                << " HiRate=" << hiRate
                << " Emerg=" << emerg
                << " Offs=" << offset
                << " Src=" << src
                << " Dst=" << dst;
        }
        else if (csbko == 0x21) // Preamble CSBKs
        {
            unsigned char groupFlag    = infoBits[17];
            unsigned char dataFlag     = infoBits[18];
            unsigned int  blocksToFollow = bitsToUint(&infoBits[24], 6);
            unsigned int  dst = bitsToUint(&infoBits[56], 24);
            msg << " BTF=" << blocksToFollow
                << " " << (groupFlag ? "G" : "U") << (dataFlag ? "D" : "V")
                << " Dst=" << dst;
        }
        else
        {
            // Generic: destination at bits[32..55], source at bits[56..79]
            unsigned int dst = bitsToUint(&infoBits[32], 24);
            unsigned int src = bitsToUint(&infoBits[56], 24);
            msg << " Src=" << src << " Dst=" << dst;
        }
    }
    else
    {
        // Non-standard: raw hex of CSBK-specific bytes (bits 16–79)
        msg << " Data=";
        unsigned char raw[8] = {0};
        for (int i = 0; i < 64; i++) raw[i / 8] |= (infoBits[16 + i] << (7 - (i % 8)));
        for (int i = 0; i < 8; i++)
        {
            msg << std::hex << std::setw(2) << std::setfill('0') << (int)raw[i];
        }
        msg << std::dec;
    }

    msg << " LB=" << (int)lb;
    const std::string logLine = msg.str();
    if (shouldLogCSBK(csbko, mfid, crcOK, logLine)) DSD_LOG(logLine);

    // Update slot text: "[act][CC] CSB [opHex] [dst7]"
    char opBuf[20];
    unsigned int dst = bitsToUint(&infoBits[(csbko == 0x21 || csbko == 0x19) ? 56 : 32], 24);
    snprintf(opBuf, sizeof(opBuf), "%02X %7u", (unsigned)csbko, dst);
    memcpy(&m_slotText[8], opBuf, 10);
}

void DSDDMR::decodeMBCHeader(const unsigned char *infoBits)
{
    unsigned char csbko = (unsigned char) bitsToUint(&infoBits[2], 6);
    unsigned char mfid  = (unsigned char) bitsToUint(&infoBits[8], 8);
    bool crcOK = csbkCRCOK(infoBits, 0xAAAA, m_verbosity); // TS 102 361-1 §B.3.12

    unsigned int dst = bitsToUint(&infoBits[32], 24);
    unsigned int src = bitsToUint(&infoBits[56], 24);

    DSD_LOG("MBC-Hdr["
        << (m_slot == DSDDMRSlot1 ? "1" : "2")
        << "] CSBKO=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)csbko
        << " MFId=0x" << std::setw(2) << (int)mfid
        << std::dec
        << " Src=" << src << " Dst=" << dst
        << (crcOK ? "" : " CRC-FAIL"));

    char opBuf[20];
    snprintf(opBuf, sizeof(opBuf), "%02X %7u", (unsigned)csbko, dst);
    memcpy(&m_slotText[8], opBuf, 10);
}

void DSDDMR::decodeMBCContinuation(const unsigned char *infoBits)
{
    // Pack 96 bits into 12 bytes and log as hex
    unsigned char raw[12] = {0};
    for (int i = 0; i < 96; i++) raw[i / 8] |= (infoBits[i] << (7 - (i % 8)));

    std::ostringstream msg;
    msg << "MBC-Cont[" << (m_slot == DSDDMRSlot1 ? "1" : "2") << "] ";
    for (int i = 0; i < 12; i++)
    {
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)raw[i];
        if (i % 4 == 3) msg << " ";
    }
    DSD_LOG(msg.str());
}

} // namespace DSDcc
