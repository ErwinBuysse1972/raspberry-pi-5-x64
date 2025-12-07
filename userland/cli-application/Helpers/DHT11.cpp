#include "Tracer/cfunctracer.h"
#include <string>
#include <vector>
#include <format>
#include <linux/gpio.h>
#include "DHT11.h"


using namespace SB::RPI5::Sensors;

CDhct11::CDhct11(std::shared_ptr<CTracer> tracer, int pin)
    : m_trace(tracer)
{
    CFuncTracer trace("CDhct11::CDhct11", m_trace);
    try
    {
        m_pio = std::make_unique<SBPio>(m_trace);
        m_pin = pin;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
CDhct11::~CDhct11()
{
    CFuncTracer trace("CDhct11::~CDhct11", m_trace);
    try
    {
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

}
bool CDhct11::Read(int& temperature, int&humidity, std::vector<std::string>& errors)
{
    CFuncTracer trace("CDhct11::Read", m_trace);
    bool bok = false;
    try
    {
        bok = m_pio->SetPulse(m_pin, 20000, 0, 0, SB::RPI5::PulseType::eNegative, false, errors);
        if (!bok)
        {
            errors.emplace_back("DHct11::Read - SetPulse failed");
            return false;
        }
        bok = m_pio->ReleasePin(m_pin, errors);
        if (!bok)
        {
            errors.emplace_back("Dhct11::Read - ReleasePin failed");
            return false;
        }

        bok = m_pio->InitEdge(m_pin, (GPIOEVENT_REQUEST_RISING_EDGE | GPIOEVENT_REQUEST_FALLING_EDGE), errors);
        if (!bok)
        {
            errors.emplace_back("Dhct11::Read - InitEdge failed");
            return false;
        }
        
        gpioevent_data ev{};

        // Expect: falling (LOW), rising (HIGH) from sensor — skip to first rising
        int attempts = 3;
        for (; attempts > 0; --attempts)
        {
            if (m_pio->WaitNextEdgeUs(2000, &ev, errors) < 0)
                errors.emplace_back("Dht11: missing first edge");

            if (ev.id == GPIOEVENT_EVENT_RISING_EDGE)
                break;
        }
        if (attempts == 0)
        {
            errors.emplace_back("DHT11: No rising edge response");
            return false;
        }

        // Read 40 data bits using RISE → FALL timing
        uint8_t bits[40]{};
        gpioevent_data riseEv = ev;
        int edgeTime = 0;
        for (int i = 0; i < 40; ++i)
        {
            gpioevent_data fallEv{};
            edgeTime = m_pio->WaitNextEdgeUs(2000, &fallEv, errors);
            if ( edgeTime < 0 ||
                fallEv.id != GPIOEVENT_EVENT_FALLING_EDGE)
            {
                errors.emplace_back("DHT11: Missing falling edge");
                return false;
            }

            unsigned long long dtNs =
                fallEv.timestamp - riseEv.timestamp;
            int dtUs = dtNs / 1000ULL;

            bits[i] = (dtUs > 50 ? 1 : 0);

            if (i < 39)
            {
                edgeTime = m_pio->WaitNextEdgeUs(2000, &riseEv, errors);
                if (( edgeTime < 0) ||
                    riseEv.id != GPIOEVENT_EVENT_RISING_EDGE)
                {
                    errors.emplace_back("DHT11: Missing next rising edge");
                    return false;
                }
            }
        }

        // Reassemble and verify checksum
        uint8_t data[5]{};
        for (int i = 0; i < 40; ++i)
            data[i / 8] = (data[i / 8] << 1) | bits[i];

        uint8_t checksum = data[0] + data[1] + data[2] + data[3];
        if (checksum != data[4])
        {
            errors.emplace_back("DHT11: Checksum mismatch");
            return false;
        }

        humidity = data[0];
        temperature = data[2];
        std::cout << "himidity :" << humidity << ", temperature : " << temperature << std::endl;
        return true;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        errors.emplace_back(std::format("DHT11: Exception occurred - %s", e.what()));
    }
    return true;
}
