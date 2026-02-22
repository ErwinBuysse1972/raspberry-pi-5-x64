#pragma once
#include <memory>
#include <string>
#include <vector>
#include <linux/gpio.h>
#include <gpiod.h>
#include "RP1Base.h"
#include "../Tracer/ctracer.h"


namespace SB::RPI5
{
    enum class pwm_mode : uint32_t
    {
        Zero = 0x0,
        TrailingEdge = 0x1,
        PhaseCorrect = 0x2,
        PDE = 0x3,
        MSBSerial = 0x4,
        PPM = 0x5,
        LeadingEdge = 0x6,
        LSBSerial = 0x7,
        Unknown = 0xFFFFFFFF,
    };

    typedef struct 
    {
        uint32_t cntrl;
        uint32_t range;
        uint32_t phase;
        uint32_t duty;
    } PWMRegs_t;

    typedef struct
    {
        uint32_t GlobalCntrl;
        uint32_t FifoCntrl;
        uint32_t CommonRange;
        uint32_t CommonDuty;
        uint32_t DutyFifo;
    } PWMGlobalRegs_t;

    typedef struct
    {
        uint32_t Pwm0_Cntrl;
        uint32_t Pwm0_DivInt;
        uint32_t Pwm0_DivFrac;
        uint32_t Pwm0_Sel;
    } PWMClockRegs_t;
    
    class RP1PWM : public RP1Base
    {
        public:
            RP1PWM(std::shared_ptr<CTracer> tracer);
            virtual ~RP1PWM();

            bool setMode(uint32_t pin, pwm_mode mode);
            bool setInvert(uint32_t pin);
            bool clrInvert(uint32_t pin);
            bool Enable(uint32_t pin);
            bool Disable(uint32_t pin);
            bool setClock(uint32_t div, uint32_t frac);
            bool setRangeDutyPhase(uint32_t pin, uint32_t range, uint32_t duty, uint32_t phase);
            bool setFrequencyDuty(uint32_t pin, uint32_t freq, int dutyPrecent);
            bool mapPin(uint32_t pin);

             
            uint32_t getPWMReg_cntrl(uint32_t pin, int pwmbase);
            uint32_t getPWMReg_range(uint32_t pin, int pwmbase);
            uint32_t getPWMReg_phase(uint32_t pin, int pwmbase);
            uint32_t getPWMReg_duty(uint32_t pin, int pwmbase);
            uint32_t getPwmClockReg_cntrl();
            uint32_t getPwmClockReg_divInt();
            uint32_t getPwmClockReg_divFrac();
            uint32_t getPwmClockReg_Sel();
            uint32_t getGlobalCntrl(int pwmBase);
            uint32_t getFifoCntrl(int pwmBase);
            uint32_t getCommonRange(int pwmBase);
            uint32_t getCommonDuty(int pwmBase);
            uint32_t getDutyFifo(int pwmBase);
            int getPwmIndex(uint32_t pin);

            
        private:
            PWMRegs_t* PwmRegs(int pwmbase = 0);
            PWMClockRegs_t* PwmClk();
            const int PWMClock;
 
            bool initClock();
            uint32_t getFunctionForPWM(uint32_t pin);
    };
}