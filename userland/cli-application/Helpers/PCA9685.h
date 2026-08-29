#pragma once
#include <memory>
#include <string>
#include <vector>
#include <linux/gpio.h>
#include "SBRP1I2C.h"
#include "../Tracer/ctracer.h"

namespace SB::RPI5
{
    class PCA9685
    {
        public:
            PCA9685(std::shared_ptr<CTracer> tracer, RP1I2C& i2c, uint8_t address = 0x40);
            virtual ~PCA9685();

            bool initialize();
            bool setPWMFrequency(float frequency);
            bool setPWM(uint8_t channel, uint16_t on, uint16_t off);
            bool setDutyCycle(uint8_t channel, float dutyCycle);
            std::string dumpRegisters(uint8_t startRegister, uint8_t endRegister);

            uint8_t getI2cAddress(){ return m_address;}

        private:
            static constexpr uint8_t MODE1       = 0x00;
            static constexpr uint8_t MODE2       = 0x01;

            static constexpr uint8_t LED0_ON_L   = 0x06;
            static constexpr uint8_t PRE_SCALE   = 0xFE;

            static constexpr uint8_t MODE1_RESTART = 0x80;
            static constexpr uint8_t MODE1_AI      = 0x20;
            static constexpr uint8_t MODE1_SLEEP   = 0x10;

            static constexpr uint8_t MODE2_OUTDRV   = 0x04;

            static constexpr uint8_t CHANNEL_COUNT = 16;

            static constexpr uint16_t PWM_STEPS = 4096;

            // Typical internal oscillator specified by PCA9685.
            static constexpr float OSCILLATOR_FREQUENCY = 25'000'000.0f;
            
            RP1I2C& m_i2c;
            uint8_t m_address;
            std::shared_ptr<CTracer> m_trace;

            const char* getRegisterName(uint8_t reg) const;
    };
}