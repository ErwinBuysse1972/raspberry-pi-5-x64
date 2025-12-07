#include "ctracer.h"
#include <cstring>
#include <algorithm>
#include <syscall.h>

using namespace std;

CTracer::CTracer(TracerLevel lvl, const char *Separator, bool bAddTimeStamp, bool bAddTraceLevelInfo, bool bUseBinDataWriting, bool PIDInfo)
    : m_bUseBinDataWriting(bUseBinDataWriting)
    , _Separator(Separator)
    , _level(lvl)
    , _bAddTimeStamp(bAddTimeStamp)
    , _bAddTraceLevelInfo(bAddTraceLevelInfo)
    , _PIDInfo(PIDInfo)
{
    try
    {
    }
    catch (...)
    {

    }
}
CTracer::CTracer(TracerLevel lvl, bool bAddTimeStamp, bool bAddTraceLevelInfo, bool bUseBinDataWriting, bool PIDInfo)
    : m_bUseBinDataWriting(bUseBinDataWriting)
    , _Separator(TRACER_DEFAULT_SEPARATOR)
    , _level(lvl)
    , _bAddTimeStamp(bAddTimeStamp)
    , _bAddTraceLevelInfo(bAddTraceLevelInfo)
    , _PIDInfo(PIDInfo)
{
}
string CTracer::GetTraceLevelInfo(TracerLevel Level)
{
    if (!_bAddTraceLevelInfo)
        return "";

    switch(Level)
    {
        case TracerLevel::TRACER_DEBUG_LEVEL:        return (string)TRACER_TRACE_LOGGING_NAME;
        case TracerLevel::TRACER_INFO_LEVEL:         return (string)TRACER_INFO_LOGGING_NAME;
        case TracerLevel::TRACER_WARNING_LEVEL:      return (string)TRACER_WARNING_LOGGING_NAME;
        case TracerLevel::TRACER_ERROR_LEVEL:        return (string)TRACER_ERROR_LOGGING_NAME;
        case TracerLevel::TRACER_FATAL_ERROR_LEVEL:  return (string)TRACER_FATAL_ERROR_LOGGING_NAME;
        case TracerLevel::TRACER_OFF_LEVEL: break;
        default: break;
    }
    return (string)"UNSPEC";
}
string CTracer::GetCurrentTimeStamp()
{
    log_watch<std::chrono::milliseconds> milli("%X.");
    std::stringstream ss;
    ss << milli;

    return ss.str();
}
string CTracer::GetCurrentPIDInfo()
{
    std::stringstream ss;

    ss <<"[" <<  getpid() << ":"<< syscall(SYS_gettid) << "] ";
    return ss.str();
}
void CTracer::Trace(const char *data)
{
    try
    {
        string outStr = GetCurrentTimeStamp() + GetTraceLevelInfo(TracerLevel::TRACER_DEBUG_LEVEL) + GetCurrentPIDInfo() +  _Separator + data;
        if (_level <= TracerLevel::TRACER_DEBUG_LEVEL)
        {
            std::lock_guard<std::recursive_mutex> lock(_mtx_Protect);
            if (!m_bUseBinDataWriting)
                Write(outStr.c_str(), TracerLevel::TRACER_DEBUG_LEVEL);
            else
                WriteBinData(outStr.c_str(), (unsigned long)outStr.length());
        }
    }
    catch(...)
    {
    }
}
void CTracer::Info(const char *data)
{
    try
    {
        string outStr = GetCurrentTimeStamp() + GetTraceLevelInfo(TracerLevel::TRACER_INFO_LEVEL) + GetCurrentPIDInfo() + _Separator + data;
        if (_level <= TracerLevel::TRACER_INFO_LEVEL)
        {
            std::lock_guard<std::recursive_mutex> lock(_mtx_Protect);
            if (!m_bUseBinDataWriting)
                Write(outStr.c_str(), TracerLevel::TRACER_INFO_LEVEL);
            else
                WriteBinData(outStr.c_str(), (unsigned long)outStr.length());
        }
    }
    catch(...)
    {
    }
}
void CTracer::Warning(const char *data)
{
    try
    {
        string outStr = GetCurrentTimeStamp() + GetTraceLevelInfo(TracerLevel::TRACER_WARNING_LEVEL) + GetCurrentPIDInfo() + _Separator + data;
        if (_level <= TracerLevel::TRACER_WARNING_LEVEL)
        {
            std::lock_guard<std::recursive_mutex> lock(_mtx_Protect);
            if (!m_bUseBinDataWriting)
                Write(outStr.c_str(), TracerLevel::TRACER_WARNING_LEVEL);
            else
                WriteBinData(outStr.c_str(), (unsigned long)outStr.length());
        }
    }
    catch(...)
    {
    }
}
void CTracer::Error(const char *data)
{
    string outStr;
    try
    {
        outStr = GetCurrentTimeStamp() + GetTraceLevelInfo(TracerLevel::TRACER_ERROR_LEVEL) + GetCurrentPIDInfo() + _Separator + data;
        if (_level <= TracerLevel::TRACER_ERROR_LEVEL)
        {
            std::lock_guard<std::recursive_mutex> lock(_mtx_Protect);
            if (!m_bUseBinDataWriting)
                Write(outStr.c_str(), TracerLevel::TRACER_ERROR_LEVEL);
            else
                WriteBinData(outStr.c_str(), (unsigned long)outStr.length());
        }
    }
    catch(...)
    {
    }
}
void CTracer::FatalError(const char *data)
{
    try
    {
        string outStr = GetCurrentTimeStamp() + GetTraceLevelInfo(TracerLevel::TRACER_FATAL_ERROR_LEVEL) + GetCurrentPIDInfo() + _Separator + data;
        if (_level <= TracerLevel::TRACER_FATAL_ERROR_LEVEL)
        {
            std::lock_guard<std::recursive_mutex> lock(_mtx_Protect);
            if (!m_bUseBinDataWriting)
                Write(outStr.c_str(), TracerLevel::TRACER_FATAL_ERROR_LEVEL);
            else
                WriteBinData(outStr.c_str(), (unsigned long)outStr.length());
        }
    }
    catch(...)
    {
    }
}

// File Tracer code
CFileTracer::CFileTracer(const std::string& directory, const std::string& fileName, TracerLevel lvl,
                         bool bAddTimeStamp, bool bTraceLevelInfo, bool bClearData, bool bPIDInfo)
    : CTracer(lvl,bAddTimeStamp, bTraceLevelInfo, false, bPIDInfo)
    , m_out(nullptr)
    , m_bClearData(bClearData)
    , _fileName(fileName)
    , _Directory(directory)
{
}
CFileTracer::~CFileTracer()
{
    try
    {
        if (m_out)
        {
            m_out->flush();
            m_out->close();
            delete m_out;
            m_out = nullptr;
        }
    }
    catch(...)
    {
    }
}
unsigned long CFileTracer::GetFileSize(void)
{
    unsigned long begin = 0, end = 0;
    unsigned long curFileSize = 0;
    try
    {
        if (m_out)
        {
            m_out->seekp(0, ios::beg);
            begin = m_out->tellp();
            m_out->seekp(0, ios::end);
            end = m_out->tellp();
            curFileSize = end - begin;
        }
    }
    catch(...)
    {

    }
    return curFileSize;
}
void CFileTracer::WriteBinData(const char *binData, unsigned long dwLen, bool bRawNoAscii)
{
    try
    {
        std::unique_ptr<char[]> buf;
        int n = 0;
        unsigned long size = 0;

        size = ((dwLen * 3) + 1);
        buf = make_unique<char[]>(size+1);
        memset(buf.get(), 0x00, (size + 1));
        for (int i = 0; i < (int)dwLen; i++)
        {
            if (bRawNoAscii)
            {
                if (binData[i] < 0x20)
                {
                    n += sprintf(buf.get() + n,"%02x.", binData[i]);
                }
                else
                {
                    n += sprintf(buf.get()+n,"%c", binData[i]);
                }
            }
            else
            {
              n += sprintf(buf.get() + n,"%02x.", binData[i]);
              if (n < 0)
                  break;
            }
        }
        Trace(buf.get());
    }
    catch(...)
    {
        Error("CFileTracer::WriteBinData - Exception occurred");
    }
}
void CFileTracer::Write(const char *data, TracerLevel /*lvl*/)
{
    try
    {
        if (m_out == nullptr)
        {
            std::string sFullfilename = _Directory + "/" + _fileName;
            if (m_bClearData == false)
            {
                m_out = new ofstream((char *)sFullfilename.c_str(), fstream::out|fstream::app );
            }
            else
            {
                m_out = new ofstream((char *)sFullfilename.c_str(), fstream::out|fstream::trunc );
            }
        }

        if (m_out == nullptr)
        {
            throw "Memory allocation failure (m_out)";
        }
        (*m_out) << data << "\n";
        m_out->flush();
    }
    catch(...)
    {

    }
}
