#include "PCA9685.h"
#include "../Tracer/cfunctracer.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>


namespace SB::RPI5
{
    PCA9685::PCA9685(std::shared_ptr<CTracer> tracer, RP1I2C& i2c, uint8_t address)
    : m_trace(tracer)
    , m_i2c(i2c)
    , m_address(address)
    {
        CFuncTracer trace("PCA9685::PCA9685", m_trace);
        try
        {
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        
    }
    PCA9685::~PCA9685()
    {
        CFuncTracer trace("PCA9685::~PCA9685", m_trace);
        try
        {
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
    }

    bool PCA9685::initialize()
    {
        CFuncTracer trace("PCA9685::initialize", m_trace);
        try
        {
            uint8_t mode1 = 0;
            if (!m_i2c.readRegsiter8(m_address, MODE1, mode1))
            {
                trace.Error("Failed to read MODE1 from PCA9685 at 0x%02X", m_address);
                return false;
            }

            trace.Info("PCA9685 MODE1 before initialization: 0x%02X", mode1);
            //Enable auto increment -> this is imported because setPWM uses a 4-byte sequential register write
            mode1 |= MODE1_AI;
            if (!m_i2c.writeRegister8(m_address, MODE1, mode1))
            {
                trace.Error("Failed to enable Auto increment on PCA9685");
                return false;
            }

            //MODE2 OutDRV = 1; configure output as totem-pole
            uint8_t mode2 = 0;
            if (!m_i2c.readRegsiter8(m_address, MODE2, mode2))
            {
                trace.Error("Failed to read MODE2 from PCA9685");
                return false;
            }

            mode2 |= MODE2_OUTDRV;
            if (!m_i2c.writeRegister8(m_address, MODE2, mode2))
            {
                trace.Error("Failed to write PCA9685 MODE2");
                return false;
            }

            uint8_t verifyMode2 = 0;

            if (!m_i2c.readRegsiter8(m_address, MODE2, verifyMode2))
            {
                trace.Error("Failed to verify PCA9685 MODE2");
                return false;
            }

            trace.Info(
                "PCA9685 MODE2 written=0x%02X readback=0x%02X",
                mode2,
                verifyMode2);

            trace.Info(
                "PCA9685 initialization at address 0x%02X, "
                "MODE1=0x%02X MODE2=0x%02X",
                m_address,
                mode1,
                mode2);
                return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool PCA9685::setPWMFrequency(float frequency)
    {
        CFuncTracer trace("PCA9685::setPWMFrequency", m_trace);
        try
        {
            if (frequency <= 0.0f)
            {
                trace.Error("Invalid PWM frequency: %f", frequency);
                return false;
            }

            // PCA9685:
            // prescale = oscillator_frequency / (4096 * frequency) - 1
            const float prescaleValue = OSCILLATOR_FREQUENCY / (static_cast<float>(PWM_STEPS) * frequency) - 1.0f;
            int prescale = static_cast<int>(prescaleValue);

            // PRE-SCALE is an 8 bit register
            prescale = std::clamp(prescale, 3, 255);
            trace.Info("Set PCA9685 PWM frequency: requested=%f Hz, prescale=%d", frequency, prescale);
            uint8_t oldMode = 0;
            if (!m_i2c.readRegsiter8(m_address, MODE1, oldMode))
            {
                trace.Error("Failed to read MODE1");
                return false;
            }

            // PRE_SCALE can only be changed while the oscillator is sleeping
            const uint8_t sleepMode = static_cast<uint8_t>((oldMode & ~MODE1_RESTART) | MODE1_SLEEP);
            if (!m_i2c.writeRegister8(m_address, MODE1, sleepMode))
            {
                trace.Error("Failed to put PCA9685 into sleep mode");
                return false;
            }

            if (!m_i2c.writeRegister8(m_address, PRE_SCALE, static_cast<uint8_t>(prescale)))
            {
                trace.Error("Failed to write PCA9685 PRE_SCALE");
                return false;
            }

            // Restore the MODE1 and ensure Auto Increment stays enable
            uint8_t wakeMode = static_cast<uint8_t>((oldMode & ~MODE1_SLEEP) | MODE1_AI);
            if (!m_i2c.writeRegister8(m_address, MODE1, wakeMode))
            {
                trace.Error("Failed to wake PCA9685");
                return false;
            }

            // Allow oscillator to stabilize
            std::this_thread::sleep_for(std::chrono::microseconds(500));

            // Restart PWM Logic
            wakeMode |= MODE1_RESTART;
            if (!m_i2c.writeRegister8(m_address, MODE1, wakeMode))
            {
                trace.Error("Failed to restart PCA9685");
                return false;
            }

            const float actualFrequency = OSCILLATOR_FREQUENCY / (static_cast<float>(PWM_STEPS) * static_cast<float>(prescale + 1));
            trace.Info("PCA9685 PWMM frequency configured: requested= %f Hz, actual= %f Hz, prescale= %d",
                frequency, actualFrequency, prescale);
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool PCA9685::setPWM(uint8_t channel, uint16_t on, uint16_t off)
    {
        CFuncTracer trace("PCA9685::setPWM", m_trace);
        try
        {
            if (channel >= CHANNEL_COUNT)
            {
                trace.Error("Invalid PCA9685 channel: %u", channel);
                return false;
            }

            if (on > PWM_STEPS || off > PWM_STEPS)
            {
                trace.Error("Invalid PWM values: channel=%u, on=%u, off=%u", channel, on, off);
                return false;
            }

            // Four registers per channel:
            //  - LEDn_ON_L
            //  - LEDn_ON_H
            //  - LEDn_OFF_L
            //  - LEDn_OFF_H
            const uint8_t startRegister = static_cast<uint8_t>(LED0_ON_L + 4 * channel);
            uint8_t data[4] = {};

            // 4096 is treated as FULL ON / FULL OFF
            if (on == PWM_STEPS)
            {
                data[0] = 0;
                data[1] = 0x10;  // FULL_ON bit;
            }
            else
            {
                data[0] = static_cast<uint8_t>(on & 0xFF);
                data[1] = static_cast<uint8_t>((on >> 8) & 0x0F);
            }

            if (off == PWM_STEPS)
            {
                data[2] = 0;
                data[3] = 0x10;  // FULL_ON bit;
            }
            else
            {
                data[2] = static_cast<uint8_t>(off & 0xFF);
                data[3] = static_cast<uint8_t>((off >> 8) & 0x0F);
            }

            if (!m_i2c.writeRegisters(m_address, startRegister, data, sizeof(data)))
            {
                trace.Error("Failed to set PWM (channel=%u on=%u off=%u)", channel, on, off);
                return false;
            }

            trace.Info("PWM channel=%u on=%u off=%u", channel, on, off);
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool PCA9685::setDutyCycle(uint8_t channel, float dutyCycle)
    {
        CFuncTracer trace("PCA9685::setDutyCycle", m_trace);
        try
        {
            if (channel >= CHANNEL_COUNT)
            {
                trace.Error("Invalid PCA9685 channel: %u", channel);
                return false;
            }

            if (dutyCycle < 0.0 || dutyCycle > 1.0f)
            {
                trace.Error("Invalid duty cycle: %f (expected range 0.0 .. 1.0)", dutyCycle);
                return false;
            }

            // special cases use the PCA9685 FULL ON/OFF bits
            if (dutyCycle <= 0.0f)
                return setPWM(channel, 0, PWM_STEPS); // FULL OFF

            if (dutyCycle >= 1.0f)
                return setPWM(channel, PWM_STEPS, 0); // FULL ON

            const uint16_t off = static_cast<uint16_t>(std::round(dutyCycle * static_cast<float>(PWM_STEPS)));
            return setPWM(channel, 0, std::min<uint16_t>(off, PWM_STEPS - 1));
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    const char* PCA9685::getRegisterName(uint8_t reg) const
    {
        switch (reg)
        {
        case 0x00: return "MODE1";
        case 0x01: return "MODE2";
        case 0x02: return "SUBADR1";
        case 0x03: return "SUBADR2";
        case 0x04: return "SUBADR3";
        case 0x05: return "ALLCALLADR";

        case 0xFA: return "ALL_LED_ON_L";
        case 0xFB: return "ALL_LED_ON_H";
        case 0xFC: return "ALL_LED_OFF_L";
        case 0xFD: return "ALL_LED_OFF_H";
        case 0xFE: return "PRE_SCALE";

        default:
            break;
        }

        // Channel registers 0x06 .. 0x45
        if (reg >= 0x06 && reg <= 0x45)
        {
            const uint8_t offset = reg - 0x06;
            const uint8_t channel = offset / 4;
            const uint8_t part = offset % 4;

            static thread_local char name[32];

            switch (part)
            {
            case 0:
                std::snprintf(name, sizeof(name),
                            "LED%u_ON_L", channel);
                break;

            case 1:
                std::snprintf(name, sizeof(name),
                            "LED%u_ON_H", channel);
                break;

            case 2:
                std::snprintf(name, sizeof(name),
                            "LED%u_OFF_L", channel);
                break;

            case 3:
                std::snprintf(name, sizeof(name),
                            "LED%u_OFF_H", channel);
                break;
            }

            return name;
        }

        return "UNKNOWN";
    }

    std::string PCA9685::dumpRegisters(uint8_t startRegister, uint8_t endRegister)
    {
        CFuncTracer trace("PCA9685::dumpRegisters", m_trace);
        try
        {
           if (startRegister > endRegister)
            {
                return std::format(
                    "Invalid PCA9685 register range: "
                    "0x{:02X}..0x{:02X}",
                    startRegister,
                    endRegister);
            }

            std::string result;

            result += std::format(
                "PCA9685 register dump "
                "(I2C address 0x{:02X})\n",
                m_address);

            result += std::format(
                "Range: 0x{:02X}..0x{:02X}\n",
                startRegister,
                endRegister);

            result +=
                "-------------------------------------------------\n";

            for (uint16_t reg = startRegister;
                reg <= endRegister;
                ++reg)
            {
                uint8_t value = 0;

                if (!m_i2c.readRegsiter8(
                        m_address,
                        static_cast<uint8_t>(reg),
                        value))
                {
                    result += std::format(
                        "0x{:02X}  {:<18} : READ ERROR\n",
                        reg,
                        getRegisterName(
                            static_cast<uint8_t>(reg)));

                    continue;
                }

                result += std::format(
                    "0x{:02X}  {:<18} : 0x{:02X}\n",
                    reg,
                    getRegisterName(
                        static_cast<uint8_t>(reg)),
                    value);
            }

            return result;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return "";
    }
}
