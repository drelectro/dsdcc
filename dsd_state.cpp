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

#include "dsd_state.h"

#pragma warning(disable : 4996)

namespace DSDcc
{

DSDState::DSDState()
{
    int i, j;

    repeat = 0;

    for (i = 0; i < 1024; i++)
    {
        maxbuf[i] = 15000;
    }

    for (i = 0; i < 1024; i++)
    {
        minbuf[i] = -15000;
    }

    midx = 0;
    sprintf(fsubtype, "              ");
    sprintf(ftype, "             ");
    symbolcnt = 0;
    lastp25type = 0;
    offset = 0;
    carrier = 0;

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 16; j++)
        {
            tg[i][j] = 48;
        }
    }

    tgcount = 0;
    lasttg = 0;
    lastsrc = 0;
    nac = 0;
    mbe_file_type = -1;
    optind = 0;
    numtdulc = 0;
    firstframe = 0;
    sprintf(slot0light, "                          ");
    sprintf(slot1light, "                          ");
//    samplesPerSymbol = 10;
    sprintf(algid, "________");
    sprintf(keyid, "________________");
    currentslot = 0;
    p25kid = 0;

    output_finished = 0;
    output_offset = 0;
    output_num_samples = 0;
    output_samples = 0;
    output_length = 0;
    output_buffer = 0;

    ccnum = 0;
}

DSDState::~DSDState()
{
}

} // namespace dsdcc
