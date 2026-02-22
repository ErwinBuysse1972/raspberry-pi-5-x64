#pragma once
#include <memory>
#include <string>
#include <cstring>
#include <chrono>
#include <functional>
#include <map>

using   Clocktype = std::chrono::high_resolution_clock;

class CScopeTimer {
private:
    const std::string m_sFuncName;
    const Clocktype::time_point m_start;
    const long long m_thrsFuncTime;
    std::function<void(const std::string& msg)> m_cbNotify;
    static std::map<std::string, Clocktype::time_point> m_recallTime;
    static std::map<std::string, float> m_totaltime;
    static std::map<std::string, long long> m_calls;
    static std::map<std::string, long long> m_funcAboveThreshold;
    std::map<std::string, Clocktype::time_point> m_interestingTimes;
public:
    CScopeTimer(const std::string& funcName, long long thrsFuncTimens, std::function<void(const std::string& msg)> cb);
    CScopeTimer(CScopeTimer&) = delete;
    CScopeTimer(CScopeTimer&&) = delete;
    auto operator=(CScopeTimer&) = delete;
    auto operator=(CScopeTimer&&) = delete;
    ~CScopeTimer();

    void SetTime(const std::string& tsName);
};
