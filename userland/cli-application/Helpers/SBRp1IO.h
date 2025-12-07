#pragma once
#include <memory>
#include <string>
#include <vector>
#include <linux/gpio.h>
#include <gpiod.h>
#include "../Tracer/ctracer.h"

namespace SB::RPI5
{

    typedef struct 
    {
        uint32_t status;
        uint32_t ctrl;
    } RP1_GPIO_Regs_t;


    typedef struct 
    {
        uint32_t Out;
        uint32_t OE;
        uint32_t In;
        uint32_t InSync;
    } RP1_Regs_t;

    enum class eGpioSlewRate
    {
        eFast,
        eSlow,
        eUnknown
    };

    enum class eGpioDrive
    {
        eCurrent_2mA,
        eCurrent_4mA,
        eCurrent_8mA,
        eCurrent_12mA,
        eUnknown
    };


    class RP1IO
    {
    public:
        RP1IO(std::shared_ptr<CTracer> tracer);
        virtual ~RP1IO();

        uint32_t getGpioStatus(uint32_t pin);
        uint32_t getGpioCntrl(uint32_t pin);
        uint32_t getRioOut();
        uint32_t getRioOutputEnable();
        uint32_t getRioIn();
        uint32_t getRioInSync();
        uint32_t getPad(uint32_t pin);
        bool setDirInMask(uint32_t mask);
        bool setDirOutMask(uint32_t mask);
        bool setDirMaskValue(uint32_t mask, uint32_t value);

        bool setGpioMask(uint32_t mask);
        bool clearGpioMask(uint32_t mask);
        bool xorGpioMask(uint32_t mask);

        bool setGpioPin(int pin, bool value);
        bool setGpioPinMasked(uint32_t mask, uint32_t value);
        bool getGpioPin(int pin);
        uint32_t getAllGpioPin();
        // Configure the internal connection (PAD configuration)
        bool setGpioPullUpPullDown(uint32_t pin, bool up, bool down);
        bool setGpioPullDown(uint32_t pin);
        bool setGpioPullUp(uint32_t pin);
        bool disabledGpioPulls(uint32_t pin);
        bool setGpioInputHysteris(uint32_t pin, bool enabled);
        bool setGpioSlewRate(uint32_t pin, eGpioSlewRate slew);
        bool setGpioDriveStrength(uint32_t pin, eGpioDrive drive);
        bool isGpioPullUp(uint32_t pin);
        bool isGpioPullDown(uint32_t pin);
        bool isGpioHysterisEnabled(uint32_t pin);
        eGpioSlewRate getGpioSlewRate(uint32_t pin);
        eGpioDrive getGpioDrive(uint32_t pin);

    private:
        std::shared_ptr<CTracer> m_trace;
        uint32_t *m_PERIBase = nullptr;
        uint32_t *m_GPIOBase = nullptr;
        uint32_t *m_RIOBase = nullptr;
        uint32_t *m_PADBase = nullptr;
        uint32_t *m_pad = nullptr;

        bool initialize();
        RP1_GPIO_Regs_t* GPIOBase();
        RP1_Regs_t* RioBase();
        RP1_Regs_t* RioXor();
        RP1_Regs_t* RioSet();
        RP1_Regs_t* RioClear();

    };
}