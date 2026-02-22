#include <memory>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <chrono>
#include <thread>
#include "../Tracer/cfunctracer.h"
#include "SBRP1Pwm.h"

namespace SB::RPI5
{
    constexpr uint32_t PAD_PWM_DEFAULT = PULL_NONE | OUTPUT_ENABLE | DRIVE_8mA | SLEW_FAST;
     

    RP1PWM::RP1PWM(std::shared_ptr<CTracer> tracer)
        : RP1Base(tracer)
        , PWMClock(50000000)
    {
        CFuncTracer trace("RP1PWM::RP1PWM", m_trace);
        try
        {
            bool bOk = initClock();
            if (!bOk)
                trace.Error("Failed to init the clock");
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
    }
    RP1PWM::~RP1PWM()
    {
        CFuncTracer trace("RP1PWM::~RP1PWM", m_trace);
        try
        {
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
    }

    PWMRegs_t* RP1PWM::PwmRegs(int pwmbase)
    {
        CFuncTracer trace("RP1PWM::PwmRegs", m_trace, false);
        uint32_t *PwmRegs = PWMBase(pwmbase) + 0x14 / 4;
        return (PWMRegs_t *)PwmRegs;
    }
    PWMClockRegs_t* RP1PWM::PwmClk()
    {
        return (PWMClockBase())? (PWMClockRegs_t*)(PWMClockBase() + 0x74 /4) : nullptr;
    }

    uint32_t RP1PWM::getFunctionForPWM(uint32_t pin)
    {
        CFuncTracer trace("RP1PWM::getFunctionForPWM", m_trace);
        try
        {
            switch(pin)
            {
                case 12: return 0;
                case 13: return 0;
                case 18: return 3;
                case 19: return 3;
                default:
                    trace.Error("Pin %ld cannot be mapped to PWM channel", pin);
                    break;
            }
            /* code */
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
        return 0xFFFFFFFFu;
    }
    
    bool RP1PWM::initClock()
    {
        CFuncTracer trace("RP1PWM::initClock", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = PwmClk();
            if (PWMCLK == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }
            PWMCLK->Pwm0_Cntrl = 0x11000840;
            PWMCLK->Pwm0_Sel = 1;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
        return false;
        
    }
    int RP1PWM::getPwmIndex(uint32_t pin)
    {
        switch(pin)
        {
            case 12: return 0;
            case 13: return 1;
            case 14: return 2;
            case 15: return 3;
            case 18: return 2;
            case 19: return 3;
            default: break;
        }
        return -1;
    }
    bool RP1PWM::setMode(uint32_t pin, pwm_mode mode)
    {
        CFuncTracer trace("RP1PWM::setMode", m_trace);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("pwmChannel not found (pin: %ld)", pin);
                return false;
            }

            PWMRegs_t * PWM = PwmRegs();
            if (PWM == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }

            bool bok = initClock();
            if (!bok)
            {
                trace.Error("initClock failed");
                return false;
            }
            setFunction(pin, getFunctionForPWM(pin), PAD_PWM_DEFAULT);
            PWM[pwmChannel].cntrl = static_cast<uint32_t>(mode);
            return Disable(pin);
        }
        catch(const std::exception& e)
        {
            trace.Error("Excception occurred : %s", e.what());
        }
        return false;
    }
   
    bool RP1PWM::setInvert(uint32_t pin)
    {
        CFuncTracer trace("RP1PWM::setInvert", m_trace);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("pwmChannel not found (pin: %ld)", pin);
                return false;
            }
            PWMRegs_t * PWM = PwmRegs();
            if (PWM == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }

           
            PWM[pwmChannel].cntrl = PWM[pwmChannel].cntrl | 0x8;
            volatile uint32_t *pwmBase = PWMBase();
            *pwmBase = *pwmBase | 0x80000000;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Excception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1PWM::clrInvert(uint32_t pin)
    {
        CFuncTracer trace("RP1PWM::clrInvert", m_trace);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("pwmChannel not found (pin: %ld)", pin);
                return false;
            }
            PWMRegs_t * PWM = PwmRegs();
            if (PWM == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }

            PWM[pwmChannel].cntrl = PWM[pwmChannel].cntrl & ~0x8ul;
            volatile uint32_t *pwmBase = PWMBase();
            *pwmBase = *pwmBase | 0x80000000;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Excception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1PWM::setClock(uint32_t div, uint32_t frac)
    {
        CFuncTracer trace("RP1PWM::setClock", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = PwmClk();   
            if (PWMCLK == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }

            PWMCLK->Pwm0_DivInt = div;
            PWMCLK->Pwm0_DivFrac = frac;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1PWM::Enable(uint32_t pin)
    {
        CFuncTracer trace("RP1PWM::Enable", m_trace);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("pwmChannel not found (pin: %ld)", pin);
                return false;
            }
            uint32_t value = 1 << pwmChannel;
            volatile uint32_t *pwmBase = PWMBase();
            *pwmBase = *pwmBase | value | 0x80000000;

            PWMRegs_t * PWM = PwmRegs();
            if (PWM == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }
            PWM[pwmChannel].cntrl |= 0x1u;

            // optional: flush if RP1 uses "apply" bit (you’ve been ORing 0x80000000 elsewhere)
            *pwmBase |= 0x80000000;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
        
    }
    bool RP1PWM::Disable(uint32_t pin)
    {
        CFuncTracer trace("RP1PWM::Disable", m_trace);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("pwmChannel not found (pin: %ld)", pin);
                return false;
            }
            volatile uint32_t *pwmBase = PWMBase();
            uint32_t value = 1 << pwmChannel;
            value = ~value & 0x0F;
            *pwmBase = (*pwmBase & value) | 0x80000000;

             PWMRegs_t * PWM = PwmRegs();
            if (PWM == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }
            PWM[pwmChannel].cntrl &= ~0x1u;

            // optional: flush if RP1 uses "apply" bit (you’ve been ORing 0x80000000 elsewhere)
            *pwmBase |= 0x80000000;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1PWM::setRangeDutyPhase(uint32_t pin, uint32_t range, uint32_t duty, uint32_t phase)
    {
        CFuncTracer trace("RP1PWM::setRangeDutyPhase", m_trace);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("pwmChannel not found (pin: %ld)", pin);
                return false;
            }
            PWMRegs_t * PWM = PwmRegs();
            if (PWM == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }

            trace.Info("PWM : %p", PWM);
            trace.Info("pwm Channel : %ld", pwmChannel);
            trace.Info("write range : %32x @address %p", range, &PWM[pwmChannel].range);
            trace.Info("write duty : %32x @address %p", duty, &PWM[pwmChannel].duty);
            trace.Info("phase : %32x @address %p ", phase, &PWM[pwmChannel].phase);

            PWM[pwmChannel].range = range;
            PWM[pwmChannel].duty = duty;
            PWM[pwmChannel].phase = phase;

            volatile uint32_t *pwmBase = PWMBase();
            *pwmBase = *pwmBase | 0x80000000;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1PWM::setFrequencyDuty(uint32_t pin, uint32_t freq, int dutyPrecent)
    {
        CFuncTracer trace("RP1PWM::setFrequencyDuty", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = PwmClk();
            if (PWMCLK == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }

            uint32_t div = PWMCLK->Pwm0_DivInt;
            uint32_t frac = PWMCLK->Pwm0_DivFrac;
            uint32_t pwmf = PWMClock / div;
            uint32_t range = pwmf / freq-1;
            uint32_t duty = range * dutyPrecent / 100;
            return setRangeDutyPhase(pin, range,  duty, 0);
        }
        catch(const std::exception& e)
        {
            trace.Error("Excception occurred : %s", e.what());
        }
        return false;        
    }

    bool RP1PWM::mapPin(uint32_t pin)
    {
        CFuncTracer trace("RP1PWM::mapPin", m_trace);
        try
        {
            /* code */
            uint32_t func = getFunctionForPWM(pin);
            if (func == 0xFFFFFFFFu)
            {
                trace.Error("func cannot get for pin %ld", pin);
                return false;
            }
            return setFunction(pin, func, PAD_PWM_DEFAULT);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        return false;
    }
    uint32_t RP1PWM::getPWMReg_cntrl(uint32_t pin, int pwmbase)
    {
        CFuncTracer trace("RP1PWM::getPWMReg_cntrl", m_trace, false);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("wrong pin (%ld)", pin);
                return static_cast<uint32_t>(-1);        
            }
            PWMRegs_t* PWM = (PWMRegs_t*)PwmRegs(pwmbase);
            trace.Info("CHAN%ld_CNTRL pwm%ld: 0x%08x, %p", pwmChannel, pwmbase, PWM[pwmChannel].cntrl, &PWM[pwmChannel].cntrl);
            return PWM[pwmChannel].cntrl;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getPWMReg_range(uint32_t pin, int pwmbase)
    {
        CFuncTracer trace("RP1PWM::getPWMReg_range", m_trace, false);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("wrong pin (%ld)", pin);
                return static_cast<uint32_t>(-1);        
            }
            PWMRegs_t* PWM = (PWMRegs_t*)PwmRegs(pwmbase);
            trace.Info("CHAN%ld_RANGE pwm%ld: 0x%08x, %p", pwmChannel, pwmbase, PWM[pwmChannel].range, &PWM[pwmChannel].range);
            return PWM[pwmChannel].range;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getPWMReg_phase(uint32_t pin, int pwmbase)
    {
        CFuncTracer trace("RP1PWM::getPWMReg_phase", m_trace, false);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("wrong pin (%ld)", pin);
                return static_cast<uint32_t>(-1);        
            }
            PWMRegs_t* PWM = (PWMRegs_t*)PwmRegs(pwmbase);
            trace.Info("CHAN%ld_PHASE pwm%ld: 0x%08x, %p", pwmChannel, pwmbase, PWM[pwmChannel].phase, &PWM[pwmChannel].phase);
            return PWM[pwmChannel].phase;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getPWMReg_duty(uint32_t pin, int pwmbase)
    {
        CFuncTracer trace("RP1PWM::getPWMReg_duty", m_trace, false);
        try
        {
            int pwmChannel = getPwmIndex(pin);
            if (pwmChannel < 0)
            {
                trace.Error("wrong pin (%ld)", pin);
                return static_cast<uint32_t>(-1); 
            }
            PWMRegs_t* PWM = (PWMRegs_t*)PwmRegs(pwmbase);
            trace.Info("CHAN%ld_DUTY pwm%ld: 0x%08x, %p", pwmChannel, pwmbase, PWM[pwmChannel].duty, &PWM[pwmChannel].duty);
            return PWM[pwmChannel].duty;

        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getGlobalCntrl(int pwmBase)
    {
        CFuncTracer trace("RP1PWM::getGlobalCntrl", m_trace, false);
        try
        {
            PWMGlobalRegs_t* GLOBAL = (PWMGlobalRegs_t*)PWMBase(pwmBase);
            trace.Info("GLOBAL_CNTRL pwm%ld: 0x%08x, %p", pwmBase, GLOBAL->GlobalCntrl, &GLOBAL->GlobalCntrl);
            return GLOBAL->GlobalCntrl;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
        
    }
    uint32_t RP1PWM::getFifoCntrl(int pwmBase)
    {
        CFuncTracer trace("RP1PWM::getFifoCntrl", m_trace, false);
        try
        {
            PWMGlobalRegs_t* GLOBAL = (PWMGlobalRegs_t*)PWMBase(pwmBase);
            trace.Info("FIFO_CNTRL pwm%ld: 0x%08x, %p", pwmBase, GLOBAL->FifoCntrl, &GLOBAL->FifoCntrl);
            return GLOBAL->FifoCntrl;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1PWM::getCommonRange(int pwmBase)
    {
        CFuncTracer trace("RP1PWM::getCommonRange", m_trace, false);
        try
        {
            PWMGlobalRegs_t* GLOBAL = (PWMGlobalRegs_t*)PWMBase(pwmBase);
            trace.Info("COMMON_RANGE pwm%ld: 0x%08x, %p", pwmBase, GLOBAL->CommonRange, &GLOBAL->CommonRange);
            return GLOBAL->CommonRange;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1PWM::getCommonDuty(int pwmBase)
    {
        CFuncTracer trace("RP1PWM::getCommonDuty", m_trace, false);
        try
        {
            PWMGlobalRegs_t* GLOBAL = (PWMGlobalRegs_t*)PWMBase(pwmBase);
            trace.Info("COMMON_DUTY pwm%ld: 0x%08x, %p", pwmBase, GLOBAL->CommonDuty, &GLOBAL->CommonDuty);
            return GLOBAL->CommonDuty;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1PWM::getDutyFifo(int pwmBase)
    {
        CFuncTracer trace("RP1PWM::getDutyFifo", m_trace, false);
        try
        {
            PWMGlobalRegs_t* GLOBAL = (PWMGlobalRegs_t*)PWMBase(pwmBase);
            trace.Info("DUTY_FIFO pwm%ld: 0x%08x, %p", pwmBase, GLOBAL->DutyFifo, &GLOBAL->DutyFifo);
            return GLOBAL->DutyFifo;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }

    uint32_t RP1PWM::getPwmClockReg_cntrl()
    {
        CFuncTracer trace("RP1PWM::getPwmClockReg_cntrl", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = (PWMClockRegs_t*)PWMClockBase();
            return PWMCLK->Pwm0_Cntrl;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getPwmClockReg_divInt()
    {
        CFuncTracer trace("RP1PWM::getPwmClockReg_divInt", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = (PWMClockRegs_t*)PWMClockBase();
            return PWMCLK->Pwm0_DivInt;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getPwmClockReg_divFrac()
    {
        CFuncTracer trace("RP1PWM::getPwmClockReg_divFrac", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = (PWMClockRegs_t*)PWMClockBase();
            return PWMCLK->Pwm0_DivFrac;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }
    uint32_t RP1PWM::getPwmClockReg_Sel()
    {
        CFuncTracer trace("RP1PWM::getPwmClockReg_Sel", m_trace);
        try
        {
            PWMClockRegs_t* PWMCLK = (PWMClockRegs_t*)PWMClockBase();
            return PWMCLK->Pwm0_Sel;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);        
    }

}