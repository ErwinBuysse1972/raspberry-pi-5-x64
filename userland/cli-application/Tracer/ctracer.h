#pragma once
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <codecvt>
#include <thread>
#include <unistd.h>

#define TRACER_DEFAULT_SEPARATOR        ((char *)" - ")
#define TRACER_DEFAULT_MAXIMUM_WAITING_FOR_SYNCHRONIZE_OBJECT           180000

#define TRACER_TRACE_LOGGING_NAME               ((char *) " TRACE     ")
#define TRACER_INFO_LOGGING_NAME                ((char *) " INFO      ")
#define TRACER_WARNING_LOGGING_NAME             ((char *) " WARNING   ")
#define TRACER_ERROR_LOGGING_NAME               ((char *) " ERROR     ")
#define TRACER_FATAL_ERROR_LOGGING_NAME         ((char *) " FAT ERROR ")

enum class TracerLevel
{
    TRACER_DEBUG_LEVEL,
    TRACER_INFO_LEVEL,
    TRACER_WARNING_LEVEL,
    TRACER_ERROR_LEVEL,
    TRACER_FATAL_ERROR_LEVEL,
    TRACER_OFF_LEVEL,
} ;

template<std::size_t V, std::size_t C = 0,
         typename std::enable_if<(V < 10), int>::type = 0>
constexpr std::size_t log10ish() {
    return C;
}

template<std::size_t V, std::size_t C = 0,
         typename std::enable_if<(V >= 10), int>::type = 0>
constexpr std::size_t log10ish() {
    return log10ish<V / 10, C + 1>();
}

template<class Precision = std::chrono::seconds,
         class Clock = std::chrono::system_clock>
class log_watch {
public:
    using precision_type = Precision;
    using ratio_type = typename precision_type::period;
    using clock_type = Clock;
    static constexpr auto decimal_width = log10ish<ratio_type{}.den>();

    static_assert(ratio_type{}.num <= ratio_type{}.den,
                  "Only second or sub second precision supported");
    static_assert(ratio_type{}.num == 1, "Unsupported precision parameter");

    // default format: "%Y-%m-%dT%H:%M:%S"
    log_watch(const std::string& format = "%FT%T") : m_format(format) {}

    template<class P, class C>
    friend std::ostream& operator<<(std::ostream&, const log_watch<P, C>&);

private:
    std::string m_format;
};

template<class Precision, class Clock>
std::ostream& operator<<(std::ostream& os, const log_watch<Precision, Clock>& lw) {
    // get current system clock
    auto time_point = Clock::now();

    // extract std::time_t from time_point
    std::time_t t = Clock::to_time_t(time_point);

    // output the part supported by std::tm
    os << std::put_time(std::localtime(&t), lw.m_format.c_str());

    // only involve chrono duration calc for displaying sub second precisions
    if(lw.decimal_width) { // if constexpr( ... in C++17
        // get duration since epoch
        auto dur = time_point.time_since_epoch();

        // extract the sub second part from the duration since epoch
        auto ss =
            std::chrono::duration_cast<Precision>(dur) % std::chrono::seconds{1};

        // output the sub second part
        os << std::setfill('0') << std::setw(lw.decimal_width) << ss.count();
    }
    return os;
}


class CTracer
{
private:
    std::recursive_mutex _mtx_Protect;
    bool m_bUseBinDataWriting;
    std::string GetCurrentPIDInfo();
protected:
    std::string _Separator;
    TracerLevel _level;
    bool _bAddTimeStamp;
    bool _bAddTraceLevelInfo;
    bool _PIDInfo;

    std::string GetCurrentTimeStamp();
public:
    CTracer(){}
    CTracer(TracerLevel lvl, const char *Separator, bool bAddTimeStamp, bool bAddTraceLevelInfo, bool bUseBinDataWriting = false, bool PIDInfo = false);
    CTracer(TracerLevel lvl, bool bAddTimeStamp, bool bAddTraceLevelInfo, bool bUseBinDataWriting = false, bool PIDInfo = false);
    CTracer(const CTracer& item)=delete;
    virtual ~CTracer() = default;

    void Trace(const char *data);
    void Info(const char *data);
    void Warning(const char *data);
    void Error(const char *data);
    void FatalError(const char *data);
    std::string GetTraceLevelInfo(TracerLevel level);

    void SetTraceLevel(TracerLevel lvl){_level = lvl;}
    TracerLevel GetTraceLevel(void){ return _level; }
    bool GetAddTimeStamp(void){return _bAddTimeStamp;}
    bool GetAddTraceLevelInfo(void){ return _bAddTraceLevelInfo;}
    bool GetPIDInfo(void){ return _PIDInfo;}

    virtual void Write(const char *data, TracerLevel lvl = TracerLevel::TRACER_DEBUG_LEVEL)=0;
    virtual void WriteBinData(const char *binData, unsigned long dwLen, bool bRawNoAscii=true)=0;
};

// File tracer class : Class that will responsible to store the
//          traces to a file on the HDD.
class CFileTracer : public CTracer
{
private:
    std::ofstream   *m_out;
    bool            m_bClearData;
protected:
    std::string     _fileName;
    std::string     _Directory;

public:
    CFileTracer(const std::string& directory, const std::string& fileName, TracerLevel lvl, bool bAddTimeStamp = true, bool bTraceLevelInfo = true, bool bClearData = false, bool bPIDInfo = false);
    CFileTracer(const CFileTracer& item) = delete;
    virtual ~CFileTracer();

    std::string GetFileName(void){ return _fileName;}
    std::string GetDirName(void){ return _Directory;}

    unsigned long GetFileSize(void);
    void Write(const char *data, TracerLevel lvl = TracerLevel::TRACER_DEBUG_LEVEL);
    void WriteBinData(const char *binData, unsigned long dwLen, bool bRawNoAscii = true);
};
