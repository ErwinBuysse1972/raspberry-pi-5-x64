#pragma once
#include <memory>
#include <string>
#include <vector>
#include <linux/gpio.h>
#include <gpiod.h>
#include "../Tracer/ctracer.h"

namespace SB::RPI5
{
    struct chipInfo
    {
        std::string name;
        std::string label;
        int lines;
    };

    enum class PulseType
    {
        ePositive,
        eNegative,
        eUnknown
    };

    class SBPio
    {
    public:
        SBPio(std::shared_ptr<CTracer> tracer);
        virtual ~SBPio();

        std::vector<chipInfo> enumChips(std::vector<std::string>& errors);

        bool CheckPinOutputSanity(int pin, std::string& reason,std::vector<std::string>& errors);
        bool SetHigh(int pin, std::vector<std::string>& errors);
        bool SetLow(int pin, std::vector<std::string>& errors);
        bool SetPulse(int pin, int widthus,int LeadPulseTimeUs, int PostPulseTimeUs, PulseType tp, bool bListen, std::vector<std::string>& errors);
        int GetInput(int pin, std::vector<std::string>& errors);
        int GetTimeRisingEdge(int pin, int timeoutus, std::vector<std::string>& errors);
        int GetTimeFallingEdge(int pin, int timeoutus, std::vector<std::string>& errors);
        int CollectNEdges(int n, int totalTimeoutUs, std::vector<int>& deltaUs, std::vector<std::string>& errors);
        int MeasRC(int pin, std::vector<std::string>& errors);

        bool ReleasePin(int pin, std::vector<std::string>& errors);
        bool InitEdge(int pin, unsigned int eventFlags, std::vector<std::string>& errors);
        int WaitNextEdgeUs(int timeoutus, gpioevent_data* outEv, std::vector<std::string>& errors);
        int WaitEdgesUs(int maxEdges, int totalTimeoutUs, std::vector<int>& deltaUs, std::vector<std::string>& errors);
    private:
        std::shared_ptr<CTracer> m_trace;
        int m_pinFd;               // line-handle fd
        int m_chipFd;              // fd for /dev/gpiochipX
        int m_eventFd;             // current line event fd (or -1 of none)
        int m_eventPin;            // which pin this event is for
        int m_pinNr;               // which pin we own
        unsigned int m_pinFlags;   // current GPIOHANDLE_REQUEST_* flags
        unsigned m_eventFlags;     // rising/falling/both 

        chipInfo makeChipInfo(gpiod_chip* chip, std::vector<std::string>& errors);
        bool ensureOutputPin(int pin, std::vector<std::string>& errors);
        bool ensureInputHighImpedance(int pin, std::vector<std::string>& errors);
        bool ensureInputPullUp(int pin, std::vector<std::string>& errors);
        bool ensureInputPullDown(int pin, std::vector<std::string>& errors);

        bool ensurePinMode(int pin, unsigned int flags, const char* label, std::vector<std::string>& errors);
        bool ReadPinRaw(int pin, bool& value, std::vector<std::string>& errors);
    };
}