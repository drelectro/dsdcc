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

#ifndef REED_SOLOMON_H_
#define REED_SOLOMON_H_

#include "export.h"

namespace DSDcc
{

class DSDCC_API ReedSolomon_24_12_13
{
public:
    ReedSolomon_24_12_13();
    ~ReedSolomon_24_12_13();
    
    bool decode(unsigned char* data, int blocks);

private:
    // Reed-Solomon implementation details would go here
    // For now, this is a stub implementation
};

} // namespace DSDcc

#endif /* REED_SOLOMON_H_ */