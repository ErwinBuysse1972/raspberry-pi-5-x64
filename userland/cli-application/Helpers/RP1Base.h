#pragma once
#include <memory>
#include "../Tracer/cfunctracer.h"

namespace SB::RPI5
{
    constexpr uint32_t CTRL_FUNCSEL_MASK = 0x1Fu; // bits 0..4

    constexpr uint32_t PULL_NONE = 0x00;
    constexpr uint32_t PULL_UP = 0x01;
    constexpr uint32_t PULL_DOWN = 0x02;

    constexpr uint32_t INPUT_ENABLE = (1u << 3);
    constexpr uint32_t OUTPUT_ENABLE = (1u << 4);

    constexpr uint32_t DRIVE_2mA = (0u << 5);
    constexpr uint32_t DRIVE_4mA = (1u << 5);
    constexpr uint32_t DRIVE_8mA = (2u << 5);
    constexpr uint32_t DRIVE_12mA = (3u << 5);

    constexpr uint32_t SLEW_FAST = (1u << 7);
    constexpr uint32_t SLEW_SLOW = 0;

    enum gpio_function_rp1: uint32_t
    {
        GPIO_FUNC_I2C = 3,
        GPIO_FUNC_PWM1 = 0,
        GPIO_FUNC_SPI = 0,
        GPIO_FUNC_PWM2 = 3,
        GPIO_FUNC_RIO = 5,
        GPIO_FUNC_NULL = 0x1f
    };

    typedef struct 
    {
        uint32_t status;
        uint32_t ctrl;
    } RP1_GPIO_Regs_t;
    
    class RP1Base
    {
        public:
            RP1Base(std::shared_ptr<CTracer> m_trace);
            virtual ~RP1Base();

            uint32_t *PERIBase();
            RP1_GPIO_Regs_t *GPIOBase();
            uint32_t *RIOBase();
            uint32_t *PADBase();
            uint32_t *pad();
            uint32_t *PWMBase(int pwmBase = 0);
            uint32_t *PWMClockBase();

            bool setFunction(uint32_t pin, uint32_t func, uint32_t pad);

        protected:
            std::shared_ptr<CTracer> m_trace;

        private:
            uint32_t *m_PERIBase = nullptr;
            uint32_t *m_GPIOBase = nullptr;
            uint32_t *m_RIOBase = nullptr;
            uint32_t *m_PADBase = nullptr;
            uint32_t *m_pad = nullptr;
            uint32_t *m_PWMBase0 = nullptr;
            uint32_t *m_PWMBase1 = nullptr;
            uint32_t *m_PWMClockBase = nullptr;
            uint32_t *m_PWMRegs = nullptr;
            
            bool initialize();
    };
}