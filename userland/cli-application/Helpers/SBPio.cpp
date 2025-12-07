#include <exception>
#include "../Tracer/cfunctracer.h"
#include "SBPio.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <cstring>      // std::strcpy
#include <chrono>
#include <format>
#include <vector>
#include <dirent.h>
#include <poll.h>
#include <time.h>

using namespace SB::RPI5;

SBPio::SBPio(std::shared_ptr<CTracer> tracer)
    : m_trace(tracer)
    , m_pinFd(-1)
    , m_pinNr(-1)
    , m_pinFlags(0)
    , m_eventFd(-1)
    , m_eventPin(-1)
    , m_eventFlags(0)
{
    CFuncTracer trace("SBPio::SBPio", m_trace);
    try
    {
        m_chipFd = ::open("/dev/gpiochip0", O_RDONLY);
        if (m_chipFd < 0)
            trace.Error("open(/dev/gpiochip0) failed : %s", std::strerror(errno));
        else
            trace.Info("m_chipFd is opened successfull : %ld", m_chipFd);
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
}

SBPio::~SBPio()
{
    CFuncTracer trace("SBPio::~SBPio", m_trace);
    try
    {
        if (m_eventFd >= 0)
        {
            ::close(m_eventFd);
            m_eventFd = -1;
        }
        if (m_chipFd >= 0)
        {
            trace.Info("Close chipFd %ld", m_chipFd);
            ::close(m_chipFd);
            m_chipFd = -1;
        } 
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
}

bool SBPio::InitEdge(int pin, unsigned int eventFlags, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::InitEdge", m_trace);
    try
    {
        if (m_chipFd < 0)
        {
            errors.emplace_back("InitEdge: chip fd invalid");
            return false;
        }

        if (  (m_eventFd >= 0)
            &&(m_eventPin == pin)
            &&(m_eventFlags == eventFlags))
        {
            return true;
        }

        // Close previous event fd
        if (m_eventFd >= 0)
        {
            ::close(m_eventFd);
            m_eventFd = -1;
            m_eventPin = -1;
            m_eventFlags = 0;
        }

        gpioevent_request ereq{};
        ereq.lineoffset = static_cast<__u32>(pin);
        ereq.handleflags = GPIOHANDLE_REQUEST_INPUT;
        ereq.eventflags = eventFlags;
        std::strncpy(ereq.consumer_label, "SBPIO Edge", sizeof(ereq.consumer_label) - 1);

        if (ioctl(m_chipFd, GPIO_GET_LINEEVENT_IOCTL, &ereq) < 0)
        {
            errors.emplace_back(std::format("InitEdge: GPIO_GET_LINEEVENT_IOCTL failed: {0}", std::strerror(errno)));
            trace.Error("InitEdge: GPIO_GET_LINEEVENT_IOCTL failed: {0}", std::strerror(errno));
            return false;
        }

        m_eventFd = ereq.fd;
        m_eventPin = pin;
        m_eventFlags = eventFlags;
        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
int SBPio::WaitNextEdgeUs(int timeoutus, gpioevent_data* outEv, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::waitNextEdgeUs", m_trace);
    try
    {
        if (m_eventFd < 0)
        {
            errors.emplace_back("WaitNextEdgeUs: no event fd initialized");
            return -1;
        }

        timespec ts_start{};
        if (clock_gettime(CLOCK_MONOTONIC, &ts_start) != 0)
        {
            errors.emplace_back(std::format("WaitNextEdgeUs: clock_gettime failed {0}", std::strerror(errno)));
            trace.Error("WaitNextEdgeUs: clock_gettime failed %s", std::strerror(errno));
            return -1;
        }
        unsigned long long start_ns =
        static_cast<unsigned long long>(ts_start.tv_sec) * 1000000000ULL +
        static_cast<unsigned long long>(ts_start.tv_nsec);

        pollfd pfd{};
        pfd.fd     = m_eventFd;
        pfd.events = POLLIN;

        int timeout_ms = (timeoutus < 0)
            ? -1
            : (timeoutus + 999) / 1000;

        int pret = ::poll(&pfd, 1, timeout_ms);
        if (pret == 0)
        {
            errors.emplace_back("WaitNextEdgeUs: timeout");
            return -1;
        }
        if (pret < 0)
        {
            errors.emplace_back(std::string("WaitNextEdgeUs: poll failed: ") +
                                std::strerror(errno));
            return -1;
        }
        if (!(pfd.revents & POLLIN))
        {
            errors.emplace_back("WaitNextEdgeUs: poll woke without POLLIN");
            return -1;
        }

        gpioevent_data ev{};
        ssize_t rd = ::read(m_eventFd, &ev, sizeof(ev));
        if (rd != sizeof(ev))
        {
            errors.emplace_back("WaitNextEdgeUs: read failed");
            return -1;
        }

        if (outEv)
            *outEv = ev;

        unsigned long long elapsed_ns =
            (ev.timestamp > start_ns) ? (ev.timestamp - start_ns) : 0ULL;

        return static_cast<int>(elapsed_ns / 1000ULL);
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;    
}
int SBPio::WaitEdgesUs(int maxEdges, int totalTimeoutUs, std::vector<int>& deltasUs, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::WaitEdgesUs", m_trace);
    using namespace std::chrono;
    try
    {
        deltasUs.clear();

        if (m_eventFd < 0)
        {
            errors.emplace_back("WaitEdgesUs: no event fd initialised");
            return -1;
        }

        // Absolute deadline for the whole capture
        auto tStart = steady_clock::now();
        auto deadline = tStart + microseconds(totalTimeoutUs);

        gpioevent_data prevEv{};
        bool havePrev = false;

        while ((int)deltasUs.size() < maxEdges)
        {
            // Remaining time for this poll
            auto now = steady_clock::now();
            if (now >= deadline)
            {
                // timeout for whole collection
                break;
            }

            auto remainingUs = duration_cast<microseconds>(deadline - now).count();
            int timeoutMs = remainingUs <= 0 ? 0 : (int)((remainingUs + 999) / 1000);

            pollfd pfd{};
            pfd.fd     = m_eventFd;
            pfd.events = POLLIN;

            int pret = ::poll(&pfd, 1, timeoutMs);
            if (pret == 0)
            {
                // no more edges before global timeout
                break;
            }
            if (pret < 0)
            {
                errors.emplace_back(std::string("WaitEdgesUs: poll failed: ") +
                                    std::strerror(errno));
                return -1;
            }
            if (!(pfd.revents & POLLIN))
            {
                errors.emplace_back("WaitEdgesUs: poll woke without POLLIN");
                return -1;
            }

            gpioevent_data ev{};
            ssize_t rd = ::read(m_eventFd, &ev, sizeof(ev));
            if (rd != sizeof(ev))
            {
                errors.emplace_back("WaitEdgesUs: read failed");
                return -1;
            }

            if (havePrev)
            {
                unsigned long long dtNs =
                    (ev.timestamp > prevEv.timestamp)
                        ? (ev.timestamp - prevEv.timestamp)
                        : 0ULL;

                deltasUs.push_back((int)(dtNs / 1000ULL));
            }

            prevEv  = ev;
            havePrev = true;
        }

        return (int)deltasUs.size();   // number of intervals we captured
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;    
}
bool SBPio::ReleasePin(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ReleasePin", m_trace);

    try
    {
        // Nothing to release
        if (m_pinFd < 0)
            return true;

        // Optional: sanity-check that we’re releasing the pin we think we own
        if (m_pinNr != -1 && m_pinNr != pin)
        {
            errors.push_back("ReleasePin: trying to release different pin than the one owned");
            trace.Error("ReleasePin: requested pin %d but currently own pin %d", pin, m_pinNr);
            return false;
        }

        close(m_pinFd);
        m_pinFd    = -1;
        m_pinNr    = -1;
        m_pinFlags = 0;

        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
bool SBPio::CheckPinOutputSanity(int pin, std::string& reason,std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::CheckPinOutputSanity", m_trace);

    reason.clear();

    try
    {
        // 1) Try to configure as OUTPUT
        if (!ensureOutputPin(pin, errors))
        {
            reason = std::format("ensureOutputPin({}) failed", pin);
            errors.push_back(reason);
            return false;
        }

        if (m_pinFd < 0)
        {
            reason = "CheckPinOutputSanity: m_pinFd is invalid after ensureOutputPin";
            errors.push_back(reason);
            return false;
        }

        gpiohandle_data data{};

        // 2) Drive HIGH and read back
        data.values[0] = 1;
        if (ioctl(m_pinFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
        {
            reason = std::format("CheckPinOutputSanity: SET HIGH failed on pin {}: {}",
                                 pin, strerror(errno));
            errors.push_back(reason);
            return false;
        }

        std::memset(&data, 0, sizeof(data));
        if (ioctl(m_pinFd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
        {
            reason = std::format("CheckPinOutputSanity: GET after HIGH failed on pin {}: {}",
                                 pin, strerror(errno));
            errors.push_back(reason);
            return false;
        }

        if (data.values[0] != 1)
        {
            reason = std::format(
                "CheckPinOutputSanity: wrote HIGH but read back {} on pin {} "
                "(line may not really be driven)",
                static_cast<int>(data.values[0]), pin);
            errors.push_back(reason);
            return false;
        }

        // 3) Drive LOW and read back
        data.values[0] = 0;
        if (ioctl(m_pinFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
        {
            reason = std::format("CheckPinOutputSanity: SET LOW failed on pin {}: {}",
                                 pin, strerror(errno));
            errors.push_back(reason);
            return false;
        }

        std::memset(&data, 0, sizeof(data));
        if (ioctl(m_pinFd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
        {
            reason = std::format("CheckPinOutputSanity: GET after LOW failed on pin {}: {}",
                                 pin, strerror(errno));
            errors.push_back(reason);
            return false;
        }

        if (data.values[0] != 0)
        {
            reason = std::format(
                "CheckPinOutputSanity: wrote LOW but read back {} on pin {} "
                "(line may still be Hi-Z or overridden)",
                static_cast<int>(data.values[0]), pin);
            errors.push_back(reason);
            return false;
        }

        // If we got here: from the GPIO subsystem’s POV, this pin behaves as a real output
        reason = std::format("Pin {} can be driven HIGH and LOW as GPIO output", pin);
        return true;
    }
    catch (const std::exception& e)
    {
        reason = std::format("CheckPinOutputSanity: exception: {}", e.what());
        errors.push_back(reason);
        trace.Error("CheckPinOutputSanity: exception: %s", e.what());
        return false;
    }
}
bool SBPio::SetPulse(int pin, int widthus, int LeadPulseTimeUs, int PostPulseTimeUs, PulseType tp, bool bListen, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::SetPulse", m_trace);

    if (widthus <= 165)
    {
        trace.Error("SetPulse: widthus must be > 155µs (got %d)", widthus);
        return false;
    }

    widthus -= 160; // your calibration

    try
    {
        if (!ensureOutputPin(pin, errors))
        {
            trace.Error("SetPulse: ensureOutputPin(%d) failed", pin);
            return false;
        }

        using namespace std::chrono;

        auto writeValue = [&](uint8_t v) -> bool {
            gpiohandle_data data{};
            data.values[0] = v;
            if (ioctl(m_pinFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
            {
                errors.emplace_back(std::format("SetPulse: GPIOHANDLE_SET_LINE_VALUES_IOCTL failed: {0}",
                                                std::strerror(errno)));
                return false;
            }
            return true;
        };

        switch (tp)
        {
        case PulseType::ePositive:
            // idle LOW -> HIGH pulse -> LOW
            if (!writeValue(0)) return false;
            if (LeadPulseTimeUs > 0) std::this_thread::sleep_for(microseconds(LeadPulseTimeUs));
            if (!writeValue(1)) return false;
            std::this_thread::sleep_for(microseconds(widthus));
            if (!writeValue(0)) return false;
            return true;

        case PulseType::eNegative:
            // idle HIGH -> LOW pulse -> HIGH
            if (!writeValue(1)) return false;
            if (LeadPulseTimeUs > 0) std::this_thread::sleep_for(microseconds(LeadPulseTimeUs));
            if (!writeValue(0)) return false;
            std::this_thread::sleep_for(microseconds(widthus));
            if (!writeValue(1)) return false;
            if (PostPulseTimeUs > 0) std::this_thread::sleep_for(microseconds(PostPulseTimeUs));
            return true;

        default:
            trace.Error("SetPulse: PulseType::eUnknown is not supported");
            return false;
        }
        if (bListen)
        {
            if (!ensureInputHighImpedance(pin, errors))
            {
                errors.emplace_back(std::format("ensureInput pullup failed for pin {0}", pin));
                return false;
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
bool SBPio::ensureOutputPin(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ensureOutputPin", m_trace);
    try
    {
         return ensurePinMode(pin, GPIOHANDLE_REQUEST_OUTPUT, "SBPio Output", errors);
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
bool SBPio::ensureInputHighImpedance(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ensureInputHighImpedance", m_trace);
    try
    {
        unsigned int flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_BIAS_DISABLE;
        return ensurePinMode(pin, flags, "SBPio Input HiZ", errors);
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
bool SBPio::ensureInputPullUp(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ensureInputPullUp", m_trace);
    try
    {
        unsigned int flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_BIAS_PULL_UP;
        return ensurePinMode(pin, flags, "SBPio Input HiZ", errors);
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
bool SBPio::ensureInputPullDown(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ensureInputPullDown", m_trace);
    try
    {
        unsigned int flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_BIAS_PULL_DOWN;
        return ensurePinMode(pin, flags, "SBPio Input HiZ", errors);
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;

}
bool SBPio::ensurePinMode(int pin, unsigned int flags, const char* label, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ensurePinMode", m_trace);

    // Already configured like this
    if (  (m_pinFd >= 0)
        &&(m_pinNr == pin) 
        &&(m_pinFlags == flags))
        return true;

    // Release previous handle (other pin or other mode)
    if (m_pinFd >= 0)
    {
        close(m_pinFd);
        m_pinFd    = -1;
        m_pinNr    = -1;
        m_pinFlags = 0;
    }

    int chipFd = open("/dev/gpiochip0", O_RDONLY);
    if (chipFd < 0)
    {
        errors.emplace_back("ensurePinMode: failed to open /dev/gpiochip0");
        return false;
    }

    gpiohandle_request req{};
    req.lineoffsets[0] = pin;
    req.lines          = 1;
    req.flags          = flags;
    std::strcpy(req.consumer_label, label);

    if (ioctl(chipFd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0)
    {
        errors.emplace_back("ensurePinMode: GPIO_GET_LINEHANDLE_IOCTL failed");
        close(chipFd);
        return false;
    }

    close(chipFd);           // only keep the line handle

    m_pinFd    = req.fd;
    m_pinNr    = pin;
    m_pinFlags = flags;

    return true;
}
bool SBPio::ReadPinRaw(int pin, bool& value, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::ReadPinRaw", m_trace);
    try
    {
        if (!ensureInputHighImpedance(pin, errors))
        {
            errors.emplace_back(std::format("Failed to configure pin {0} as input", pin));
            return false;
        }

        gpiohandle_data data{};
        if(ioctl(m_pinFd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
        {
            errors.emplace_back(std::format("GPIOHANDLE_GET_LINE_VALUES_IOCTL failed: {0}", std::strerror(errno)));
            return false;
        }
        value = (data.values[0] != 0);
        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;
}
bool SBPio::SetHigh(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::SetHigh", m_trace);
    try
    {
        if (!ensureOutputPin(pin, errors))
        {
            errors.emplace_back(std::format("Failed to configure pin {0} as output", pin));
            return false;
        }

        gpiohandle_data data{};
        data.values[0] = 1;

        if (ioctl(m_pinFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
        {
            errors.emplace_back("SetHigh: GPIOHANDLE_SET_LINE_VALUES_IOCTL failed");
            return false;
        }

        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;    
}
bool SBPio::SetLow(int pin, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::SetLow", m_trace);
    try
    {
        if (!ensureOutputPin(pin, errors))
        {
            errors.emplace_back(std::format("Failed to configure pin {0} as output", pin));
            return false;
        }

        gpiohandle_data data{};
        data.values[0] = 0;

        if (ioctl(m_pinFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
        {
            errors.emplace_back("SetHigh: GPIOHANDLE_SET_LINE_VALUES_IOCTL failed");
            return false;
        }

        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return false;

}
int SBPio::GetInput(int pin, std::vector<std::string>& errors)
{
   CFuncTracer trace("SBPio::GetInput", m_trace);
    try
    {
        bool value = false;
        if (!ReadPinRaw(pin, value, errors))
            return -1; // -1 = error

        return value ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;
}
int SBPio::GetTimeRisingEdge(int pin, int timeoutus, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::GetTimeRisingEdge", m_trace);
    try
    {
        if (!InitEdge(pin, GPIOEVENT_EVENT_RISING_EDGE, errors))
            return -1;

        return WaitNextEdgeUs(timeoutus, nullptr, errors);
    }
    catch (const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;
}
int SBPio::GetTimeFallingEdge(int pin, int timeoutus, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::GetTimeFallingEdge", m_trace);

    try
    {
        if (!InitEdge(pin, GPIOEVENT_EVENT_FALLING_EDGE, errors))
            return -1;

        return WaitNextEdgeUs(timeoutus, nullptr, errors);
    }
    catch (const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;
}
int SBPio::CollectNEdges(int n, int totalTimeoutUs, std::vector<int>& deltasUs, std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::CollectNEdges", m_trace);
    try
    {
        deltasUs.clear();
        gpioevent_data prev{}, ev{};
        bool havePrev = false;

        auto start = std::chrono::steady_clock::now();
        auto deadline = start + std::chrono::microseconds(totalTimeoutUs);

        for (int i = 0; i < n; ++i)
        {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                break;

            auto remainingUs =
                std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();

            int dt = WaitNextEdgeUs((int)remainingUs, &ev, errors);
            if (dt < 0)
                break;

            if (havePrev)
            {
                unsigned long long dtns =
                    (ev.timestamp > prev.timestamp) ? (ev.timestamp - prev.timestamp) : 0ULL;
                deltasUs.push_back((int)(dtns / 1000ULL));
            }

            prev    = ev;
            havePrev = true;
        }

        return (int)deltasUs.size();
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;
}
int SBPio::MeasRC(int pin, std::vector<std::string>& errors)
{
     CFuncTracer trace("SBPio::MeasRC", m_trace);

    // Config
    constexpr long long TIMEOUT_US = 500'000;           // 500 ms timeout
    timespec dischargeDelay { 0, 50 * 1000 * 1000 };    // 50 ms discharge
    timespec pollDelay      { 0,  1 * 1000 * 1000 };    // 1 ms between polls

    try
    {
        // 1) Discharge: pin as output LOW
        if (!ensureOutputPin(pin, errors))
        {
            errors.emplace_back(std::format("MeasRC: ensureOutputPin({0}) failed", pin));
            return -1;
        }

        if (!SetLow(pin, errors))
        {
            errors.emplace_back(std::format("MeasRC: SetLow({0}) failed", pin));
            return -1;
        }

        nanosleep(&dischargeDelay, nullptr);

        // 2) Measurement: pin as input, high impedance (no internal pulls)
        if (!ensureInputHighImpedance(pin, errors))
        {
            errors.emplace_back(std::format("MeasRC: ensureInputHighImpedance({0}) failed", pin));
            return -1;
        }

        // m_pinFd now refers to an INPUT handle for this pin
        if (m_pinFd < 0)
        {
            errors.emplace_back("MeasRC: m_pinFd invalid after ensureInputHighImpedance");
            return -1;
        }

        // Start timing
        timespec t1{}, t2{};
        clock_gettime(CLOCK_MONOTONIC, &t1);

        gpiohandle_data data{};
        if (ioctl(m_pinFd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
        {
            errors.emplace_back("MeasRC: initial GPIOHANDLE_GET_LINE_VALUES_IOCTL failed");
            return -1;
        }

        bool timeout = false;

        while (data.values[0] == 0) // still low
        {
            clock_gettime(CLOCK_MONOTONIC, &t2);

            long      sec_diff  = t2.tv_sec  - t1.tv_sec;
            long      nsec_diff = t2.tv_nsec - t1.tv_nsec;
            long long elapsed_us =
                sec_diff * 1'000'000LL + nsec_diff / 1'000LL;

            if (elapsed_us >= TIMEOUT_US)
            {
                timeout = true;
                break;
            }

            if (ioctl(m_pinFd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
            {
                errors.emplace_back("MeasRC: GPIOHANDLE_GET_LINE_VALUES_IOCTL failed in loop");
                return -1;
            }

            nanosleep(&pollDelay, nullptr);
        }

        if (timeout)
        {
            errors.emplace_back("MeasRC: timeout waiting for RC charge on pin %d", pin);
            return -1;
        }

        // We’ve just seen the input go HIGH; take final timestamp
        clock_gettime(CLOCK_MONOTONIC, &t2);

        long      sec_diff   = t2.tv_sec  - t1.tv_sec;
        long      nsec_diff  = t2.tv_nsec - t1.tv_nsec;
        long long total_nsec = sec_diff * 1'000'000'000LL + nsec_diff;
        long      usec       = total_nsec / 1'000;

        printf("MeasRC pin %d: %.02f ms\n", pin, total_nsec / 1e6);
        return static_cast<int>(usec);
    }
    catch (const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return -1;
}
std::vector<chipInfo> SBPio::enumChips(std::vector<std::string>& errors)
{
    CFuncTracer trace("SBPio::enumChips", m_trace);
    try
    {
       std::vector<chipInfo> chips;

        DIR* dir = opendir("/dev");
        if (!dir)
        {
            errors.emplace_back("could not open /dev");
            return std::vector<chipInfo>(); // or throw
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string path = std::string("/dev/") + entry->d_name;

            // Check if this is a GPIO chip device (new recommended helper)
            if (!gpiod_is_gpiochip_device(path.c_str()))
                continue;

            gpiod_chip* chip = gpiod_chip_open(path.c_str());
            if (chip)
                chips.push_back(makeChipInfo(chip, errors));
            // else: handle error if you care
        }
        closedir(dir);
        return chips;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
        errors.emplace_back(std::format("exception occurred : {0}", e.what()));
    }
    return std::vector<chipInfo>();
}
chipInfo SBPio::makeChipInfo(gpiod_chip* chip, std::vector<std::string>& errors)
{
    chipInfo info{};

    struct gpiod_chip_info* cinfo = gpiod_chip_get_info(chip);
    if (!cinfo)
    {
        // Return default-initialized info on error
        errors.emplace_back("Could not get the chipInfo (cinfo == nullptr)");
        return info;
    }

    const char* name  = gpiod_chip_info_get_name(cinfo);
    const char* label = gpiod_chip_info_get_label(cinfo);
    unsigned int lines = gpiod_chip_info_get_num_lines(cinfo);

    info.name  = name  ? name  : "";
    info.label = label ? label : "";
    info.lines = static_cast<int>(lines);

    gpiod_chip_info_free(cinfo);
    return info;
}