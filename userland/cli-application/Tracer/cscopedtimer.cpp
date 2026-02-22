#include <cscopedtimer.h>
#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <chrono>

using   Clocktype = std::chrono::high_resolution_clock;

std::map<std::string, Clocktype::time_point> CScopeTimer::m_recallTime;
std::map<std::string, float> CScopeTimer::m_totaltime;
std::map<std::string, long long> CScopeTimer::m_calls;
std::map<std::string, long long> CScopeTimer::m_funcAboveThreshold;

CScopeTimer::CScopeTimer(const std::string& funcName, long long thrsFuncTimens, std::function<void(const std::string& msg)> cb)
    : m_sFuncName(funcName)
    , m_start(Clocktype::now())
    , m_thrsFuncTime(thrsFuncTimens)
    , m_cbNotify(cb)
{
    if (m_calls.find(m_sFuncName) == m_calls.end())
    {
        m_calls.insert(std::make_pair(m_sFuncName, 1));
    }
    else
    {
       ++m_calls[m_sFuncName];
    }
    if (m_funcAboveThreshold.find(m_sFuncName) == m_funcAboveThreshold.end())
      m_funcAboveThreshold.insert(std::make_pair(m_sFuncName, 0));
}
CScopeTimer::~CScopeTimer()
{
    auto funcTime = Clocktype::now() - m_start;
    std::stringstream ss;

    if (std::chrono::duration<float, std::nano>(funcTime).count() > m_thrsFuncTime)
    {
        ++m_funcAboveThreshold[m_sFuncName];
    }

    ss << std::endl;
    ss << "#calls : " << m_calls[m_sFuncName] << std::endl;
    ss << "#calls above thrshd : " << m_funcAboveThreshold[m_sFuncName] << std::endl;
    ss << "func time : " << std::chrono::duration<float, std::milli>(funcTime).count() << " ms" << std::endl;
    ss << "time between two calls : ";
    if (m_recallTime.find(m_sFuncName) != m_recallTime.end())
    {
          ss << std::chrono::duration<float, std::nano>(m_start - m_recallTime[m_sFuncName]).count() << " ns" << std::endl;
    }
    else
    {
          ss << "0 ns" << std::endl;
    }
    if (m_totaltime.find(m_sFuncName) == m_totaltime.end())
    {
        m_totaltime.insert(std::make_pair(m_sFuncName, std::chrono::duration<float, std::nano>(funcTime).count()));
    }
    else
    {
        m_totaltime[m_sFuncName] += std::chrono::duration<float, std::nano>(funcTime).count();
    }

    ss << "total processing time : " << m_totaltime[m_sFuncName] << " ns." <<  std::endl;

    for (auto& ts : m_interestingTimes)
    {
        auto duration = ts.second - m_start;
        float fdur = std::chrono::duration<float, std::micro>(duration).count();
        ss << "    " << ts.first << " : " << fdur << " us" << std::endl;
    }


    if (m_cbNotify)
        m_cbNotify(ss.str());

    if (m_recallTime.find(m_sFuncName)!= m_recallTime.end())
        m_recallTime[m_sFuncName] = Clocktype::now();
    else
        m_recallTime.insert(std::make_pair(m_sFuncName, Clocktype::now()));
}
void CScopeTimer::SetTime(const std::string& tsName)
{

    if (m_interestingTimes.find(tsName)!= m_interestingTimes.end())
        m_interestingTimes[tsName] = Clocktype::now();
    else
        m_interestingTimes.insert(std::make_pair(tsName, Clocktype::now()));
}
