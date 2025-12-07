#include <memory>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include "../Tracer/cfunctracer.h"
#include "SBRp1IO.h"

namespace SB::RPI5
{
    RP1IO::RP1IO(std::shared_ptr<CTracer> tracer)
    : m_trace(tracer)
    , m_PERIBase(nullptr)
    , m_GPIOBase(nullptr)
    , m_RIOBase(nullptr)
    , m_PADBase(nullptr)
    , m_pad(nullptr)
    {
        CFuncTracer trace("RP1IO::RP1IO", m_trace);
        try
        {
            bool bok = initialize();
            if (!bok)
                trace.Error("initialize failed");
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
    }
    RP1IO::~RP1IO()
    {
        CFuncTracer trace("RP1IO::RP1IO", m_trace);
        try
        {
            
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
    }
    bool RP1IO::initialize()
    {
        CFuncTracer trace("RP1IO::initialize", m_trace);
        try
        {
            int memfd = open("/dev/mem", O_RDWR | O_SYNC);
            uint32_t* map = (uint32_t*)mmap(
                nullptr,
                64*1024*1024,
                (PROT_READ | PROT_WRITE),
                MAP_SHARED,
                memfd, 
                0x1f00000000); 
            close(memfd);
            m_PERIBase = map;
            if (map == MAP_FAILED)
            {
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
            return true;
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        return false;
    }
    RP1_GPIO_Regs_t* RP1IO::GPIOBase()
    {
        return (RP1_GPIO_Regs_t*)m_GPIOBase;
    }
    RP1_Regs_t* RP1IO::RioBase()
    {
        return (RP1_Regs_t*)m_RIOBase;
    }
    RP1_Regs_t* RP1IO::RioXor()
    {
        return (m_RIOBase)? (RP1_Regs_t*)(m_RIOBase + 0x1000 /4) : nullptr;
    }
    RP1_Regs_t* RP1IO::RioSet()
    {
        return (m_RIOBase)? (RP1_Regs_t*)(m_RIOBase + 0x2000 /4): nullptr;
    }
    RP1_Regs_t* RP1IO::RioClear()
    {
        return (m_RIOBase)? (RP1_Regs_t*)(m_RIOBase + 0x3000 /4): nullptr;
    }

    bool RP1IO::setDirInMask(uint32_t mask)
    {
        CFuncTracer trace("RP1IO::setDirInMask", m_trace);
        try
        {
            if (RioClear() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioClear = nullptr)");
                return false;
            }
            RioClear()->OE = mask;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception ocuurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setDirOutMask(uint32_t mask)
    {
        CFuncTracer trace("RP1IO::setDirOutMask", m_trace);
        try
        {
            if (RioSet() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioSet = nullptr)");
                return false;
            }
            RioSet()->OE = mask;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception ocuurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setDirMaskValue(uint32_t mask, uint32_t value)
    {
        CFuncTracer trace("RP1IO::setDirMaskValue", m_trace);
        try
        {
            if (RioXor() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioXor = nullptr)");
                return false;
            }
            RioXor()->OE = (RioBase()->Out ^ value) & mask;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception ocuurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioMask(uint32_t mask)
    {
        CFuncTracer trace("RP1IO::setGpioMask", m_trace);
        try
        {
            if (RioSet() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioSet = nullptr)");
                return false;
            }

            RioSet()->Out = mask;
            return true;
        }
         catch(const std::exception& e)
        {
            trace.Error("Exception ocuurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::clearGpioMask(uint32_t mask)
    {
        CFuncTracer trace("RP1IO::clearGpioMask", m_trace);
        try
        {
            if (RioClear() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioClear = nullptr)");
                return false;
            }
            RioClear()->Out = mask;
        }
         catch(const std::exception& e)
        {
            trace.Error("Exception ocuurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::xorGpioMask(uint32_t mask)
    {
        CFuncTracer trace("RP1IO::xorGpioMask", m_trace);
        try
        {
            if (RioXor() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioXor = nullptr)");
                return false;
            }
            RioXor()->Out = mask;
        }
         catch(const std::exception& e)
        {
            trace.Error("Exception ocuurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioPin(int pin, bool value)
    {
        CFuncTracer trace("RP1IO::SetGpioPin", m_trace);
        try
        {
            uint32_t mask = 1ul << pin;
            if(value)
                return setGpioMask(mask);
            else
                return clearGpioMask(mask);
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioPinMasked(uint32_t mask, uint32_t value)
    {
        CFuncTracer trace("RP1IO::SetGpioPinMasked", m_trace);
        try
        {
            if ((RioXor() == nullptr) || (RioBase() == nullptr))
            {
                trace.Error("RP1IO is not initialized (RioXor = nullptr)");
                return false;
            }


            RioXor()->Out = (RioBase()->Out ^ value) & mask;
            return true;            
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;

    }
    bool RP1IO::getGpioPin(int pin)
    {
        CFuncTracer trace("RP1IO::GetGpioPin", m_trace);
        try
        {
            if (RioBase() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioXor = nullptr)");
                return false;
            }

            return RioBase()->In & (1u << pin);
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    uint32_t RP1IO::getAllGpioPin()
    {
        CFuncTracer trace("RP1IO::GetAllGpioPin", m_trace);
        try
        {
            if (RioBase() == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioClear = nullptr)");
                return (uint32_t) -1;
            }
            return RioBase()->In;            
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return (uint32_t)-1;
    }
    bool RP1IO::setGpioPullUpPullDown(uint32_t pin, bool up, bool down)
    {
        CFuncTracer trace("RP1IO::setGpioPullUpPullDown", m_trace);
        try
        {
            m_pad[pin] &= ~0xC;
            if (up) setGpioPullUp(pin);
            if (down) setGpioPullDown(pin);
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioPullDown(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::setGpioPullDown", m_trace);
        try
        {
              if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            m_pad[pin] &= ~0xC;
            m_pad[pin] |= 0x4;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioPullUp(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::setGpioPullUp", m_trace);
        try
        {
             if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            m_pad[pin] &= ~0xC;
            m_pad[pin] |= 0x8;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::disabledGpioPulls(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::disabledGpioPulls", m_trace);
        try
        {
             if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            m_pad[pin] &= ~0xC;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioInputHysteris(uint32_t pin, bool enabled)
    {
        CFuncTracer trace("RP1IO::setGpioInputHysteris", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            m_pad[pin] &= ~0x02;
            if (enabled) m_pad[pin] |= 0x02;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioSlewRate(uint32_t pin, eGpioSlewRate slew)
    {
        CFuncTracer trace("RP1IO::setGpioSlewRate", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            m_pad[pin] &= ~0x01;
            if (slew == eGpioSlewRate::eFast)
                m_pad[pin] |= 0x01;
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::setGpioDriveStrength(uint32_t pin, eGpioDrive drive)
    {
        CFuncTracer trace("RP1IO::setGpioDriveStrength", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            m_pad[pin] &= ~0x30;
            switch(drive)
            {
                case eGpioDrive::eCurrent_2mA: break;
                case eGpioDrive::eCurrent_4mA: m_pad[pin] |= 0x10; break;
                case eGpioDrive::eCurrent_8mA: m_pad[pin] |= 0x20; break;
                case eGpioDrive::eCurrent_12mA: m_pad[pin] |= 0x30; break;

                default:
                    trace.Error("Unknown drive : %ld", drive);
                    break;
            }
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::isGpioPullUp(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::isGpioPullUp", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            return m_pad[pin] & 0x08 == 0x08;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::isGpioPullDown(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::isGpioPullDown", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            return m_pad[pin] & 0x04 == 0x04;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1IO::isGpioHysterisEnabled(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::isGpioHysterisEnabled", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return false;
            }
            return m_pad[pin] & 0x02 == 0x02;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    eGpioSlewRate RP1IO::getGpioSlewRate(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::getGpioSlewRate", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return eGpioSlewRate::eUnknown;
            }
            return (m_pad[pin] & 0x01 == 0x1)? eGpioSlewRate::eFast : eGpioSlewRate::eSlow;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return eGpioSlewRate::eUnknown;
    }
    eGpioDrive RP1IO::getGpioDrive(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::getGpioDrive", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return eGpioDrive::eUnknown;
            }
            return (m_pad[pin] & 0x30 == 0x00)? eGpioDrive::eCurrent_2mA :
                   (m_pad[pin] & 0x30 == 0x01)? eGpioDrive::eCurrent_4mA :
                   (m_pad[pin] & 0x30 == 0x02)? eGpioDrive::eCurrent_8mA :
                   (m_pad[pin] & 0x30 == 0x30)? eGpioDrive::eCurrent_12mA :
                   eGpioDrive::eUnknown;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return eGpioDrive::eUnknown;
    }

    uint32_t RP1IO::getGpioStatus(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::getGpioStatus", m_trace);
        try
        {
            RP1_GPIO_Regs_t* pGpioBase = GPIOBase();
            if (pGpioBase == nullptr)
            {
                trace.Error("RP1IO is not initialized (GPIOBase = nullptr)");
                return static_cast<uint32_t>(-1);
            }
            uint32_t status = pGpioBase[pin].status;
            trace.Info("status : 0x%p", status);
            return status;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1IO::getGpioCntrl(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::getGpioCntrl", m_trace);
        try
        {
            RP1_GPIO_Regs_t* pGpioBase = GPIOBase();
            if (pGpioBase == nullptr)
            {
                trace.Error("RP1IO is not initialized (GPIOBase = nullptr)");
                return (uint32_t)-1;
            }
            return pGpioBase[pin].ctrl;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1IO::getRioOut()
    {
        CFuncTracer trace("RP1IO::getRioOut", m_trace);
        try
        {
            RP1_Regs_t* pRioBase = RioBase();
            if (pRioBase == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioBase = nullptr)");
                return static_cast<uint32_t>(-1);
            }
            return pRioBase->Out;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1IO::getRioOutputEnable()
    {
        CFuncTracer trace("RP1IO::getRioOutputEnable", m_trace);
        try
        {
            RP1_Regs_t* pRioBase = RioBase();
            if (pRioBase == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioBase = nullptr)");
                return static_cast<uint32_t>(-1);
            }
            return pRioBase->OE;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1IO::getRioIn()
    {
        CFuncTracer trace("RP1IO::getRioIn", m_trace);
        try
        {
            RP1_Regs_t* pRioBase = RioBase();
            if (pRioBase == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioBase = nullptr)");
                return static_cast<uint32_t>(-1);
            }
            return pRioBase->In;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1IO::getRioInSync()
    {
        CFuncTracer trace("RP1IO::getRioInSync", m_trace);
        try
        {
            RP1_Regs_t* pRioBase = RioBase();
            if (pRioBase == nullptr)
            {
                trace.Error("RP1IO is not initialized (RioBase = nullptr)");
                return static_cast<uint32_t>(-1);
            }
            return pRioBase->InSync;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    }
    uint32_t RP1IO::getPad(uint32_t pin)
    {
        CFuncTracer trace("RP1IO::getPad", m_trace);
        try
        {
            if (m_pad == nullptr)
            {
                trace.Error("RP1IO is not initialized (m_pad = nullptr)");
                return static_cast<uint32_t>(-1);
            }
            uint32_t pad = m_pad[pin];
            trace.Info("pad : 0x%p", pad);
            return pad;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return static_cast<uint32_t>(-1);
    
    }
}