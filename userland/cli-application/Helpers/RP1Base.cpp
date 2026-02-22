#include "RP1Base.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>

namespace SB::RPI5
{
    RP1Base::RP1Base(std::shared_ptr<CTracer> tracer)
    : m_trace(tracer)
    {
        CFuncTracer trace("RP1Base::RP1Base", m_trace);
        try
        {
            bool bOk = initialize();
            if (!bOk)
                trace.Error("initialize failed");
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
    }
    RP1Base::~RP1Base()
    {
        CFuncTracer trace("RP1Base::~RP1Base", m_trace);
        try
        {
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
    }

    uint32_t *RP1Base::PERIBase(){ return m_PERIBase;}
    RP1_GPIO_Regs_t *RP1Base::GPIOBase(){ return (RP1_GPIO_Regs_t*)m_GPIOBase;}
    uint32_t *RP1Base::RIOBase(){ return m_RIOBase;}
    uint32_t *RP1Base::PADBase(){ return m_PADBase;}
    uint32_t *RP1Base::pad(){ return m_pad;}
    uint32_t *RP1Base::PWMBase(int pwmBase){ return (pwmBase == 0)? m_PWMBase0: m_PWMBase1;}
    uint32_t *RP1Base::PWMClockBase(){ return m_PWMClockBase;}

    bool RP1Base::initialize()
    {
        CFuncTracer trace("RP1Base::initialize", m_trace);
        try
        {
            constexpr off_t RP1_BAR0_BASE = 0x1f00000000ULL;
            constexpr size_t RP1_BAR0_SIZE = 0x00400000; // 4 MB

            int memfd = open("/dev/mem", O_RDWR | O_SYNC);
            uint32_t* map = (uint32_t*)mmap(
                nullptr,
                RP1_BAR0_SIZE,
                (PROT_READ | PROT_WRITE),
                MAP_SHARED,
                memfd, 
                RP1_BAR0_BASE); 
            close(memfd);
            m_PERIBase = map;
            if (map == MAP_FAILED)
            {
                trace.Info("opens gpiomem0 as fallback");
                int memfd = open("/dev/gpiomem0", O_RDWR, O_SYNC);
                uint32_t *map = (uint32_t *) mmap(
                    nullptr,
                    576 * 1024,
                    (PROT_READ | PROT_WRITE),
                    MAP_SHARED, 
                    memfd,
                    0x0);
                close(memfd);
                if (map == MAP_FAILED)
                {
                    trace.Error("mmap failed : %s", strerror(errno));
                    return false;
                }
                m_PERIBase = map - 0xD0000 / 4;
            }

            m_GPIOBase = m_PERIBase + 0xD0000 / 4;
            m_RIOBase = m_PERIBase + 0xe0000 / 4;
            m_PADBase = m_PERIBase + 0xf0000 / 4;
            m_pad = m_PADBase + 1; // PADBase + 0 = Voltage select register
            m_PWMBase0 = m_PERIBase + 0x98000 / 4;  // change 98000 to 9C000
            m_PWMBase1 = m_PERIBase + 0x9c000 / 4;
            m_PWMClockBase = m_PERIBase + 0x18000 / 4;


            trace.Info("PERIBase : %p", m_PERIBase);
            trace.Info("GPIO_BASE : %p", m_GPIOBase);
            trace.Info("RIO_BASE : %p", m_RIOBase);
            trace.Info("PAD_BASE : %p", m_PADBase);
            trace.Info("PAD : %p", m_pad);
            trace.Info("PWM_BASE0 : %p", m_PWMBase0);
            trace.Info("PWM_BASE1 : %p", m_PWMBase1);
            trace.Info("PWM_CLOCK_BASE : %p", m_PWMClockBase);
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1Base::setFunction(uint32_t pin, uint32_t func, uint32_t pad)
    {
        CFuncTracer trace("RP1PWM::setFunction", m_trace);
        try
        {
            RP1_GPIO_Regs_t* GPIO = (RP1_GPIO_Regs_t*)m_GPIOBase;
            if (GPIO == nullptr)
            {
                trace.Error("PWM is not correctly initialized");
                return false;
            }
            m_pad[pin] = pad;

            uint32_t v = GPIO[pin].ctrl;
            v = (v & ~CTRL_FUNCSEL_MASK) | (func & CTRL_FUNCSEL_MASK);
            GPIO[pin].ctrl = v;

            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;        
    }

}