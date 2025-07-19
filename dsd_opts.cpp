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

#include "dsd_opts.h"

namespace DSDcc
{

DSDOpts::DSDOpts()
{
    onesymbol = 10;
    errorbars = 1;
    symboltiming = 0;
    verbose = 2;
    dmr_bp_key = 0;
    p25enc = 0;
    p25lc = 0;
    p25status = 0;
    p25tg = 0;
    scoperate = 15;
    playoffset = 0;
    audio_gain = 0;
    audio_out = 1;
    resume = 0;
    frame_dstar = 0;
    frame_x2tdma = 0;
    frame_p25p1 = 0;
    frame_nxdn48 = 0;
    frame_nxdn96 = 0;
    frame_dmr = 0;
    frame_provoice = 0;
    frame_dpmr = 0;
    frame_ysf = 0;
    uvquality = 3;
    inverted_x2tdma = 1; // most transmitter + scanner + sound card combinations show inverted signals for this
    delay = 0;
    use_cosine_filter = 1;
    unmute_encrypted_p25 = 0;
}

DSDOpts::~DSDOpts()
{
}

} // namespace dsdcc

