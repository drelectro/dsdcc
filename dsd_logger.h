
///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
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

#ifndef DSD_LOGGER_H_
#define DSD_LOGGER_H_

#include <sdkddkver.h>
#include <afx.h>

#include <stdio.h>
#include <cstdarg>

#include <streambuf>
#include <ostream>
#include <windows.h>
#include <sstream>
#include <iostream>

#include "export.h"

#include "../XMC_SDR.h"


class dbg_stream_for_cout
    : public std::stringbuf
{
public:
    ~dbg_stream_for_cout() { sync(); }
    int sync()
    {
        ::OutputDebugStringA(str().c_str());
        CString s(str().c_str());
        if(s.GetLength())
            theApp.LogTextToRXView(s);
        str(std::string()); // Clear the string buffer
        return 0;
    }
};

class CXMCSDRApp;
extern CXMCSDRApp theApp;

namespace DSDcc
{
/*
class DebugOutput : public std::streambuf
{
protected:
    virtual std::streamsize xsputn(const char* s, std::streamsize n)
    {
        OutputDebugStringA(std::string(s, n).c_str());
        return n;
    }

    virtual int_type overflow(int_type c = traits_type::eof())
    {
        char z = c;
        OutputDebugStringA(std::string(&z, 1).c_str());
        return c;
    }
};

DebugOutput debugOutput;
std::ostream debugStream(&debugOutput);
*/



class DSDCC_API DSDLogger
{
public:
    DSDLogger();
    explicit DSDLogger(const char *filename);
    ~DSDLogger();

    void setFile(const char *filename);
    void setVerbosity(int verbosity) { m_verbosity = verbosity; }

    void log(const char* fmt, ...) const
    {
        if (m_verbosity > 0)
        {
            char buffer[1024];
            va_list argptr;
            va_start(argptr, fmt);
            //vfprintf(m_logfp, fmt, argptr);
            _vsnprintf_s(buffer, sizeof(buffer) / sizeof(char), _TRUNCATE, fmt, argptr);
            va_end(argptr);

            OutputDebugStringA(buffer);
        }
    }

    dbg_stream_for_cout g_DebugStreamFor_cout;
    

private:
    FILE *m_logfp;
    int  m_verbosity;
};

} // namespace DSDcc

#endif /* DSD_LOGGER_H_ */
