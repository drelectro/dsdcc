
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

#include <stdio.h>
#include <cstdarg>

#include <streambuf>
#include <ostream>
#ifndef NOMINMAX
#define NOMINMAX   // keep windows.h from defining min/max function-like macros
#endif
#include <winsock2.h>  // Must precede windows.h to avoid WinSock.h/WinSock2.h conflict
#include <windows.h>
#ifdef interface
#undef interface  // windows.h defines 'interface' as a keyword; restore it as a normal identifier
#endif
#ifdef small
#undef small      // rpcndr.h (via windows.h) defines 'small' as 'char'; prevents use as an identifier
#endif
// If another header included windows.h first (without NOMINMAX), the macros
// already exist — scrub them like 'interface'/'small' above. Every std::min/
// std::max in a TU that includes this header breaks otherwise; the project
// only survived because Qt's qendian.h happened to #undef them at a lucky
// point in the include order (any include reshuffle broke the build).
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <sstream>
#include <iostream>

#include "export.h"

// Internal helper — implemented in dsd_logger.cpp, routes to LogRouter.
// Used by the TRACE macro so the header stays Qt-free.
void dsd_trace_post(const char* msg);

// Redirect TRACE to the Qt log widget (was MFC macro, then OutputDebugStringA)
#ifndef TRACE
#ifdef _DEBUG
#define TRACE(fmt, ...) do { char _tbuf[512]; _snprintf_s(_tbuf, sizeof(_tbuf), _TRUNCATE, fmt, ##__VA_ARGS__); dsd_trace_post(_tbuf); } while(0)
#else
#define TRACE(fmt, ...) do {} while(0)
#endif
#endif

// Thread-safe log: builds the entire message atomically before posting.
// Use instead of bare std::cerr chains, which are not thread-safe across
// multiple DSP threads sharing the same redirected streambuf.
// Usage:  DSD_LOG("value=" << x << " hex=0x" << std::hex << y);
#define DSD_LOG(stream_expr) \
    do { std::ostringstream _dsd_oss; _dsd_oss << stream_expr; dsd_trace_post(_dsd_oss.str().c_str()); } while(0)

class dbg_stream_for_cout
    : public std::stringbuf
{
public:
    ~dbg_stream_for_cout() { sync(); }
    int sync() override;
};

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

    void log(const char* fmt, ...) const;

    dbg_stream_for_cout g_DebugStreamFor_cout;
    

private:
    FILE *m_logfp;
    int  m_verbosity;
};

} // namespace DSDcc

#endif /* DSD_LOGGER_H_ */
