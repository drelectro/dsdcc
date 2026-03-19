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

#include "dsd_logger.h"
#include "../Utils/LogRouter.h"

#pragma warning(disable : 4996)

// Route cout/cerr output to the Qt log widget.
int dbg_stream_for_cout::sync()
{
    if (!str().empty()) {
        LogRouter::post(str().c_str());
        str(std::string());
    }
    return 0;
}

// Used by the TRACE macro — keeps LogRouter out of the header.
void dsd_trace_post(const char* msg)
{
    LogRouter::post(msg);
}

namespace DSDcc
{

DSDLogger::DSDLogger()
{
    m_verbosity = 1;
    m_logfp = stderr;
    std::cout.rdbuf(&g_DebugStreamFor_cout); // Redirect std::cout to LogRouter
    std::cerr.rdbuf(&g_DebugStreamFor_cout); // Redirect std::cerr to LogRouter
}

DSDLogger::DSDLogger(const char *filename)
{
    m_verbosity = 1;
    m_logfp = fopen(filename, "w");

    if (!m_logfp) {
        m_logfp = stderr;
    }
}

DSDLogger::~DSDLogger()
{
    if (m_logfp != stderr) {
        fclose(m_logfp);
    }
}

void DSDLogger::setFile(const char *filename)
{
    if (m_logfp != stderr) {
        fclose(m_logfp);
    }

    m_logfp = fopen(filename, "w");

    if (!m_logfp) {
        m_logfp = stderr;
    }
}

void DSDLogger::log(const char* fmt, ...) const
{
    if (m_verbosity > 0)
    {
        char buffer[1024];
        va_list argptr;
        va_start(argptr, fmt);
        _vsnprintf_s(buffer, sizeof(buffer) / sizeof(char), _TRUNCATE, fmt, argptr);
        va_end(argptr);
        LogRouter::post(buffer);
    }
}

} // namespace DSDcc
