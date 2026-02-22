#include <cfunctracer.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


using namespace std;

void CFuncTracer::trace(const string& message)
{
    try
    {
        if (_functionName != "")
        {
            string dataToSend = _functionName + CFUNCTRACER_SEPARATOR + message;
            _tracer->Trace(dataToSend.c_str());
        }
        else
            _tracer->Trace(message.c_str());
    }
    catch(...)
    {}
}
void CFuncTracer::info(const string& message)
{
    try
    {
        if (_functionName != "")
        {
            string dataToSend = _functionName + CFUNCTRACER_SEPARATOR + message;
            _tracer->Info(dataToSend.c_str());
        }
        else
            _tracer->Info(message.c_str());
    }
    catch(...)
    {}
}
void CFuncTracer::warning(const string& message)
{
    try
    {
        if (_functionName != "")
        {
            string dataToSend = _functionName + CFUNCTRACER_SEPARATOR + message;
            _tracer->Warning(dataToSend.c_str());
        }
        else
            _tracer->Warning(message.c_str());
    }
    catch(...)
    {}
}
void CFuncTracer::error(const string& message)
{
    try
    {
        if (_functionName != "")
        {
            string dataToSend = _functionName + CFUNCTRACER_SEPARATOR + message;
            _tracer->Error(dataToSend.c_str());
        }
        else
            _tracer->Error(message.c_str());
    }
    catch(...)
    {}

}
void CFuncTracer::fatalError(const string& message)
{
    try
    {
        if (_functionName != "")
        {
            string dataToSend = _functionName + CFUNCTRACER_SEPARATOR + message;
            _tracer->FatalError(dataToSend.c_str());
        }
        else
            _tracer->FatalError(message.c_str());
    }
    catch(...)
    {}
}
void CFuncTracer::Trace(const char* fmt, ...)
{
    va_list arg_ptr;
    std::unique_ptr<char[]> buffer;

    try
    {
        buffer = make_unique<char[]>(TRACER_MAX_BUFFER_SIZE + 1);
        memset(buffer.get(), 0x00, TRACER_MAX_BUFFER_SIZE + 1);
        va_start(arg_ptr, fmt);
        vsnprintf(buffer.get(), TRACER_MAX_BUFFER_SIZE, fmt, arg_ptr);
        va_end(arg_ptr);

        trace(buffer.get());
    }
    catch(...)
    {
    }
}
void CFuncTracer::Info(const char* fmt, ...)
{
    va_list arg_ptr;
    try
    {
        unique_ptr<char[]> buffer = make_unique<char[]>(TRACER_MAX_BUFFER_SIZE + 1);
        memset(buffer.get(), 0x00, TRACER_MAX_BUFFER_SIZE + 1);
        va_start(arg_ptr, fmt);
        vsnprintf(buffer.get(), TRACER_MAX_BUFFER_SIZE, fmt, arg_ptr);
        va_end(arg_ptr);
        info(buffer.get());
    }
    catch(...)
    {
    }
}
void CFuncTracer::Warning(const char* fmt, ...)
{
    va_list arg_ptr;
    std::unique_ptr<char[]> buffer;

    try
    {
        buffer = std::make_unique<char[]>(TRACER_MAX_BUFFER_SIZE + 1);
        memset(buffer.get(), 0x00, TRACER_MAX_BUFFER_SIZE + 1);
        va_start(arg_ptr, fmt);
        vsnprintf(buffer.get(), TRACER_MAX_BUFFER_SIZE, fmt, arg_ptr);
        va_end(arg_ptr);

        warning(buffer.get());
    }
    catch(...)
    {
    }

}
void CFuncTracer::Error(const char* fmt, ...)
{
    va_list arg_ptr;
    std::unique_ptr<char[]> buffer;

    try
    {
        buffer = std::make_unique<char[]>(TRACER_MAX_BUFFER_SIZE + 1);
        memset(buffer.get(), 0x00, TRACER_MAX_BUFFER_SIZE + 1);
        va_start(arg_ptr, fmt);
        vsnprintf(buffer.get(), TRACER_MAX_BUFFER_SIZE, fmt, arg_ptr);
        va_end(arg_ptr);

        error( buffer.get());
    }
    catch(...)
    {
    }

}
void CFuncTracer::FatalError(const char* fmt, ...)
{
    va_list arg_ptr;
    std::unique_ptr<char[]> buffer;

    try
    {
        buffer = std::make_unique<char[]>(TRACER_MAX_BUFFER_SIZE + 1);
        memset(buffer.get(), 0x00, TRACER_MAX_BUFFER_SIZE + 1);
        va_start(arg_ptr, fmt);
        vsnprintf(buffer.get(), TRACER_MAX_BUFFER_SIZE, fmt, arg_ptr);
        va_end(arg_ptr);

        fatalError(buffer.get());
    }
    catch(...)
    {
    }
}
char CFuncTracer::TraceGetAsciiChar(unsigned char byte)
{
    char chReturn = '.';

    if (((byte >= '0') && (byte <= '9'))
        || ((byte >= 'A') && (byte <= 'Z'))
        || ((byte >= 'a') && (byte <= 'z'))
        )
        chReturn = (char)byte;
    return chReturn;
}
void CFuncTracer::LogDataBuffer(unsigned char *lpData, unsigned long Length, const char *logName)
{
    unsigned long len;
    try
    {
        Info("%s : ", logName);
        Info("    [%s] Length : %ld", logName, Length);
        len = (Length * 8);

        std::unique_ptr<char[]> lpDataStrHex = make_unique<char[]>(len);
        memset(lpDataStrHex.get(), 0x00, len);

        std::unique_ptr<char[]> lpDataStrChar = make_unique<char[]>(len);
        memset(lpDataStrChar.get(), 0x00, len);

        Info("    [%s] lpbData (0x%p):", logName, lpData);
        int iPos = 0;
        for (int i = 0, icPos = 0; i < (int)Length; i++)
        {
            iPos += sprintf(lpDataStrHex.get() + iPos, " 0x%02x", lpData[i]);
            icPos += sprintf(lpDataStrChar.get() + icPos, "%c", TraceGetAsciiChar(lpData[i]));
            if ((i % 8 == 0)
                && (i != 0))
            {
                Trace("    %s  -  %s", lpDataStrHex.get(), lpDataStrChar.get());
                memset(lpDataStrHex.get(), 0x00, len);
                memset(lpDataStrChar.get(), 0x00, len);
                iPos = 0;
                icPos = 0;
            }
        }

        if (iPos != 0)
            Trace("    %s  -  %s", lpDataStrHex.get(), lpDataStrChar.get());
    }
    catch (...)
    {
    }
}
