#include "Helpers/CLIParameters.h"
#include "Helpers/Metrics.h"
#include "Helpers/string_ext.h"
#include "Helpers/SBPio.h"
#include "Helpers/cout_ext.h"
#include "Helpers/DHT11.h"
#include "Helpers/SBRp1IO.h"
#include "Helpers/SBRP1Pwm.h"
#include "Tracer/cfunctracer.h"
#include "Tracer/ctracer.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <exception>
#include <readline/readline.h>
#include <readline/history.h>

using std::cout;
using std::cerr;
using std::endl;

std::unordered_map<std::string, MOW::Statistics::MetricValue> m_Metrics;
std::shared_ptr<CFileTracer> tracer = std::make_shared<CFileTracer>("./", "cliApplication.log", TracerLevel::TRACER_DEBUG_LEVEL);
std::unique_ptr<SB::RPI5::RP1IO> GpioRegisters = nullptr;
std::unique_ptr<SB::RPI5::RP1PWM> PwmRegisters = nullptr;
int pinNr = -1;
SB::RPI5::SBPio ioPin(tracer);

enum class eCmd
{
    eUnknown,
    eShell,
    eSetHigh,
    eSetLow,
    eSetPulse,
    eGetInput,
    eIsPinFree,
    eMeasRC,
	testParameterParsing,
	enumchips,

    // GPIO Commands
    eGetGpioStatus,
    eGetGpioCntrl,
    eGetRpioOut,
    eGetRpioOE,
    eGetRpioIn,
    eGetRpioInSync,
    eGetPAD,
    eFastClk,
    ePinSetFunction,

    // PWM Commands
    ePwmGetRegGlobal,
    ePwmGetReg,
    ePwmGetClk,
    ePwmSetClk,
    ePwmSetRangeDutyPhase,
    ePwmSetFrequencyDuty,
    ePwmEnable,
    ePwmDisable,
    ePwmSetMode,
    ePwmSetInvert,
    ePwmClearInvert,

    readDHT11,
    eQuit
};

void LogParameters(MOW::Application::CLI::ArgumentParseResult& pars)
{
    cout << "Command      : " << pars.command << endl;
    cout << "#options     : " << pars.options.size() << endl;
    for (auto& s : pars.options)
        cout << "    - " << s.first << " = " << s.second << endl;

    cout << "#flags       : " << pars.flags.size() << endl;
    for (auto& f : pars.flags)
        cout << "    - " << f << endl;

    cout << "#positionals : " << pars.positionals.size() << endl;
    for (auto& p : pars.positionals)
        cout << "    - " << p << endl;

    cout << FRed << "#errors      : " << pars.errors.size() << endl;
    for (auto& e : pars.errors)
        cout << FRed <<"    - " << e << endl;

	cout << FWhite;
}
void LogErrors(std::vector<std::string>& errors)
{
    CFuncTracer trace("LogErrors", tracer, false);
    try
    {
        if (errors.size())
        {
            cout << FRed << "#Errors: " << errors.size() << endl;
            trace.Error("#Errors: %ld", errors.size());
            for(auto e : errors)
            {
                cout << FRed << "   - " << e << endl;
                trace.Error("    - %s", e.c_str());
            }
            cout << FWhite;
        }

    }
    catch(std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }

}

eCmd GetCommand(std::string& command)
{
	CFuncTracer trace("GetCommands", tracer, false);
    std::string sLower = command;
    std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

    if (sLower.find("shell") != std::string::npos) return eCmd::eShell;
    if (sLower.find("quit")  != std::string::npos) return eCmd::eQuit;
    if (sLower.find("sethigh") != std::string::npos) return eCmd::eSetHigh;
    if (sLower.find("setlow")  != std::string::npos) return eCmd::eSetLow;
    if (sLower.find("setpulse") != std::string::npos) return eCmd::eSetPulse;
    if (sLower.find("measrc")  != std::string::npos) return eCmd::eMeasRC;
	if (sLower.find("testparameterparsing") != std::string::npos) return eCmd::testParameterParsing;
	if (sLower.find("enumchips") != std::string::npos) return eCmd::enumchips;
    if (sLower.find("ispinfree") != std::string::npos) return eCmd::eIsPinFree;
    if (sLower.find("getinput") != std::string::npos) return eCmd::eGetInput;
    if (sLower.find("readdht11") != std::string::npos) return eCmd::readDHT11;
    if (sLower.find("getgpiostatus") != std::string::npos) return eCmd::eGetGpioStatus;
    if (sLower.find("getgpiocntrl") != std::string::npos) return eCmd::eGetGpioCntrl;
    if (sLower.find("getrpioout") != std::string::npos) return eCmd::eGetRpioOut;
    if (sLower.find("getrpiooe") != std::string::npos) return eCmd::eGetRpioOE;
    if (sLower.find("getrpioinsync") != std::string::npos) return eCmd::eGetRpioInSync;
    if (sLower.find("getrpioin") != std::string::npos) return eCmd::eGetRpioIn;
    if (sLower.find("getpad") != std::string::npos) return eCmd::eGetPAD;
    if (sLower.find("fastclk") != std::string::npos) return eCmd::eFastClk;
    if (sLower.find("pwmgetregglobal") != std::string::npos) return eCmd::ePwmGetRegGlobal;
    if (sLower.find("pwmgetreg") != std::string::npos) return eCmd::ePwmGetReg;
    if (sLower.find("pwmgetclk") != std::string::npos) return eCmd::ePwmGetClk;
    if (sLower.find("pwmsetclk") != std::string::npos) return eCmd::ePwmSetClk;
    if (sLower.find("pwmsetrange") != std::string::npos) return eCmd::ePwmSetRangeDutyPhase;
    if (sLower.find("pwmsetfreq") != std::string::npos) return eCmd::ePwmSetFrequencyDuty;
    if (sLower.find("pwmenable") != std::string::npos) return eCmd::ePwmEnable;
    if (sLower.find("pwmdisable") != std::string::npos) return eCmd::ePwmDisable;
    if (sLower.find("pwmsetmode") != std::string::npos) return eCmd::ePwmSetMode;
    if (sLower.find("pwmsetinvert") != std::string::npos) return eCmd::ePwmSetInvert;
    if (sLower.find("pwmclearinvert") != std::string::npos) return eCmd::ePwmClearInvert;
    if (sLower.find("setfunction") != std::string::npos) return eCmd::ePinSetFunction;
    return eCmd::eUnknown;
}

void Usage(const std::vector<std::string>& errors)
{
    CFuncTracer trace("Usage", tracer, false);
    cout << "**********************************************************************************************" << endl;
    cout << "   Application v1.0.0" << endl;
    cout << "**********************************************************************************************" << endl;
    cout << "commands:" << endl;
    cout << "    - shell :  starts a command shell that you can send different commands" << endl;
    cout << "    - quit  :  quits the shell" << endl;
    cout << "    - setHigh : set the pin high (mandatory --pin)" << endl;
    cout << "    - setLow  : set the pin low (mandatory --pin)" << endl;
    cout << "    - getinput : gets the input of the pin" << endl;
    cout << "    - setpulse : generates a positive or negative pulse on a GPIO (min 180µs) (mandatory --pin, --width)" << endl;
    cout << "                 (optional -listen)" << endl;
    cout << "    - ispinfree : checks if pin is free (mandatory --pin)" << endl;
    cout << "    - measrc  : measure the time that a capa is loaded when the io pin is released (mandatory --pin)" << endl;
	cout << "    - enumschips : enumerate the chips" << endl;
	cout << "    - testparameterparsing: logs the parameters given with this command" << endl;
    cout << "    - readdht11 : reads the temperature and humidity" << endl;
    cout << "    - getgpiostatus : get the status register of specific GPIO pin (mandatory --pin)" << endl;
    cout << "    - getgpiocntrl : get the control register of specific GPIO pin (mandatory --pin)" << endl;
    cout << "    - getrpioout : get the RIO_OUT register for all IO pins" << endl;
    cout << "    - getrpiooe : get the RIO_OE register for all IO pins" << endl;
    cout << "    - getrpioinsync : get the RIO_INSYNC register for all IO pins" << endl;
    cout << "    - getrpioin : get the RIO_IN register for all IO pins" << endl;
    cout << "    - getpad : get the PAD register of specific GPIO pin (mandatory --pin)" << endl;
    cout << "    - setfunction: set the pad and function for a specific pin (mandatory --pin, --pad, --func)" << endl;
    cout << "    - fastclk : generates a fast clk (mandatory --pin, --periods)" << endl;
    cout << "    - pwmgetregglobal: get pwm global registers (optional --base)" << endl;
    cout << "    - pwmgetreg: get pwm registrs (mandatory --pin, optional --base)" << endl;
    cout << "    - pwmgetclk: get the pwm clock registers" << endl;
    cout << "    - pwmsetclk: set the pwm clock (mandatory --div, --pin and --frac)" << endl;
    cout << "    - pwmsetrange: set the range/phase/duty of the pwm signal (mandatory --pin, --range, --duty and --phase)" << endl;
    cout << "    - pwmsetfreq: set the frequency of the pwm signal (mandatory --pin, --freq and --duty)" << endl;
    cout << "    - pwmenable: enable the pwm (mandatory --pin)" << endl;
    cout << "    - pwmdisable: disable the pwm (mandatory --pin)" << endl;
    cout << "    - pwmsetmode: set the mode of the pwm (mandatory --pin, --pwmmode)" << endl;
    cout << "    - pwmsetinvert: invert the pwm signal (mandatory --pin)" << endl;
    cout << "    - pwmclearinvert: clear the inversion of the signal (mandatory --pin)" << endl;
    cout << "options:" << endl;
    cout << "    --pin=<number> : give the pin you want to perform the actions" << endl;
    cout << "    --pad=<padvalue> : gives the hw configuration for specicif pin" << endl;
    cout << "    --func=<a-value> : is the a value (0-8) you can give" << endl;
    cout << "    --width=time   : set the width time in us." << endl;
    cout << "    --leadtime=time : is the time that the level is initial set before the pulse is generated" << endl;
    cout << "    --posttime=time : is the time that the level is kept after the pulse is generated" << endl;
    cout << "    --periods=periods : is the number of periods that are generated" << endl;
    cout << "    --div=divider : this gives the divider value for the pwm clock" << endl;
    cout << "    --frac=fraction : this give the fraction for the pwm clock" << endl;
    cout << "    --freq=freq : this gives the frequency of the signal" << endl;
    cout << "    --range=range: is the period range of the pwm signal" << endl;
    cout << "    --duty=duty : is the duty in precent that the pwm signal should take" << endl;
    cout << "    --phase=phase : is the phase that the pwm signal should take" << endl;
    cout << "    --pwmmode=mode : set the mode of the pwm (zero, trailing, edging, phasecorrect, pde, ppm, msb, lsb)" << endl;
    cout << "    --base=baseNr: pwm of the rp1 contains two different pwm channels pwm0 (0) and pwm1 (1)" << endl;
    cout << "flags:" << endl;
    cout << "     -positive : positive pulse/edge" << endl;
    cout << "     -negative : negative pulse/edge" << endl;
    cout << "     -listen : goto input mode after pulse" << endl;
    cout << "     -registry: use the Rp1 registers" << endl;
    cout << endl;
    cout << "**********************************************************************************************" << endl;
    cout << "  copyright @ 2025 - MOW Vlaanderen" << endl;
    cout << "**********************************************************************************************" << endl;

    if (!errors.empty())
    {
        cout <<FRed <<  "Errors:" << endl;
        trace.Error("#Errors : ");
        for (const auto& e : errors)
        {
            cout << FRed <<  "  - " << e << endl;
            trace.Error("   - %s", e.c_str());
        }
		cout << FWhite;

    }
}

void logRegister(uint32_t reg, std::string title)
{
    cout << title << endl;
    // Header row: bit indices
    cout << "Bit : ";
    for (int i = 31; i >= 0; --i)
    {
        cout << std::setw(2) << i << ' ';
    }
    cout << " | HEX\n";

    // Value row: bit values
    cout << "Val : ";
    for (int i = 31; i >= 0; --i)
    {
        uint32_t bit = (reg >> i) & 0x1;
        cout << ' ' << bit << ' ';
    }

    // Hex column
    cout << " | 0x"
              << std::hex << std::uppercase
              << std::setw(8) << std::setfill('0') << reg
              << std::dec << std::setfill(' ')
              << '\n';
}

bool cmdPwmGetGlobal(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& error)
{
    CFuncTracer trace("cmdPwmGetRegisters", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        int base = 0;
        auto itBase = options.find("base");
        bool bHasBase = (itBase != options.end());
		if (bHasBase)
            base = std::stoi(itBase->second);

        uint32_t global_cntrl = PwmRegisters->getGlobalCntrl(base);
        uint32_t fifo_cntrl = PwmRegisters->getFifoCntrl(base);
        uint32_t common_range = PwmRegisters->getCommonRange(base);
        uint32_t common_duty = PwmRegisters->getCommonDuty(base);
        uint32_t duty_fifo = PwmRegisters->getDutyFifo(base);
    
        logRegister(global_cntrl, std::format("GLOCAL_CTRL pwm{0} ", base));
        logRegister(fifo_cntrl,   std::format("FIFO_CTRL pwm{0}   ", base));
        logRegister(common_range, std::format("COMMON_RANGE pwm{0}", base));
        logRegister(common_duty,  std::format("COMMON_DUTY pwm{0} ", base));
        logRegister(duty_fifo,    std::format("DUTY_FIFI pwm{0}   ", base));
        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;

}
bool cmdPwmGetRegisters(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmGetRegisters", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        int base = 0;
        auto itPin = options.find("pin");
        auto itBase = options.find("base");

        bool bHasBase = (itBase != options.end());
        bool bHasPin = (itPin != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
        if (bHasBase)
            base = std::stoi(itBase->second);

        int pin = std::stoi(itPin->second);
        uint32_t control = PwmRegisters->getPWMReg_cntrl(pin, base);
        uint32_t range = PwmRegisters->getPWMReg_range(pin, base);
        uint32_t phase = PwmRegisters->getPWMReg_phase(pin, base);
        uint32_t duty = PwmRegisters->getPWMReg_duty(pin, base);
        int pwmChannel = PwmRegisters->getPwmIndex(pin);
    
        logRegister(control, std::format("PWM{0} CHAN{1}_CNTRL", base, pwmChannel));
        logRegister(range,   std::format("PWM{0} CHAN{1}_RANGE", base, pwmChannel));
        logRegister(phase,   std::format("PWM{0} CHAN{1}_PHASE", base, pwmChannel));
        logRegister(duty,    std::format("PWM{0} CHAN{1}_DUTY ", base, pwmChannel));
        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
}
bool cmdPwmGetClockRegisters(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmGetClockRegisters", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);

        uint32_t control = PwmRegisters->getPwmClockReg_cntrl();
        uint32_t divFrac = PwmRegisters->getPwmClockReg_divFrac();
        uint32_t divInt = PwmRegisters->getPwmClockReg_divInt();
        uint32_t sel = PwmRegisters->getPwmClockReg_Sel();
    
        logRegister(control,    std::format("PWM{0} control", 0));
        logRegister(sel,        std::format("PWM{0} sel    ", 0));
        logRegister(divFrac,    std::format("PWM{0} divFrac", 0));
        logRegister(divInt,     std::format("PWM{0} divInt ", 0));
        return true;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
}
bool cmdPwmSetFrequency(const std::unordered_map<std::string, std::string>&options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmSetFrequency", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        auto itFreq = options.find("freq");
        auto itDuty = options.find("duty");

        bool bHasPin = (itPin != options.end());
        bool bHasFreq = (itFreq != options.end());
        bool bHasDuty = (itDuty != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
        if (!bHasFreq)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the freq option"));
			return false;
		}
        if (!bHasDuty)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the duty option"));
			return false;
		}

        int pin = std::stoi(itPin->second);
        int freq = std::stoi(itFreq->second);
        int duty = std::stoi(itDuty->second);

        bool bok = PwmRegisters->setFrequencyDuty(pin, freq, duty);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
    
}
bool cmdPwmSetRange(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmSetRange", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        auto itRange = options.find("range");
        auto itDuty = options.find("duty");
        auto itPhase = options.find("phase");

        bool bHasPin = (itPin != options.end());
        bool bHasRange = (itRange != options.end());
        bool bHasDuty = (itDuty != options.end());
        bool bHasPhase = (itPhase != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
        if (!bHasRange)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the range option"));
			return false;
		}
        if (!bHasDuty)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the duty option"));
			return false;
		}
        if (!bHasPhase)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the phase option"));
			return false;
		}

        int pin = std::stoi(itPin->second);
        int range = std::stoi(itRange->second);
        int duty = std::stoi(itDuty->second);
        int phase = std::stoi(itPhase->second);

        bool bok = PwmRegisters->setRangeDutyPhase(pin, range, duty, phase);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
    
}
bool cmdPwmEnable(const std::unordered_map<std::string, std::string>&options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmEnable", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        bool bHasPin = (itPin != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}

        int pin = std::stoi(itPin->second);
        bool bok = PwmRegisters->Enable(pin);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
}
bool cmdPwmDisable(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmDisable", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        bool bHasPin = (itPin != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}

        int pin = std::stoi(itPin->second);
        bool bok = PwmRegisters->Disable(pin);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;    
}
bool cmdPwmSetMode(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmSetMode", tracer);
    try
    {
         if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        auto itMode = options.find("pwmmode");

        bool bHasPin = (itPin != options.end());
        bool bHasPwmMode = (itMode != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
        if (!bHasPwmMode)
        {
            errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pwm mode option"));
            return false;
        }

        int pin = std::stoi(itPin->second);
        SB::RPI5::pwm_mode mode = SB::RPI5::pwm_mode::Unknown;
        //zero, trailing, edging, phasecorrect, pde, ppm, msb, lsb
        if (itMode->second.find("zero") != std::string::npos) mode = SB::RPI5::pwm_mode::Zero;
        else if (itMode->second.find("trailing") != std::string::npos) mode = SB::RPI5::pwm_mode::TrailingEdge;
        else if (itMode->second.find("edging") != std::string::npos) mode = SB::RPI5::pwm_mode::LeadingEdge;
        else if (itMode->second.find("phasecorrect") != std::string::npos) mode = SB::RPI5::pwm_mode::PhaseCorrect;
        else if (itMode->second.find("pde") != std::string::npos) mode = SB::RPI5::pwm_mode::PDE;
        else if (itMode->second.find("ppm") != std::string::npos) mode = SB::RPI5::pwm_mode::PPM;
        else if (itMode->second.find("msb") != std::string::npos) mode = SB::RPI5::pwm_mode::MSBSerial;
        else if (itMode->second.find("lsb") != std::string::npos) mode = SB::RPI5::pwm_mode::LSBSerial;
        else
        {
            errors.emplace_back(std::format("SYNTAX-ERROR : mode is not valid {0}", itMode->second));
            return false;
        }

        bool bok = PwmRegisters->setMode(pin, mode);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
    
}
bool cmdPwmSetInvert(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmSetInvert", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        bool bHasPin = (itPin != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}

        int pin = std::stoi(itPin->second);
        bool bok = PwmRegisters->setInvert(pin);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
}
bool cmdPwmClearInvert(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPwmSetInvert", tracer);
    try
    {
        if (PwmRegisters == nullptr)
            PwmRegisters = std::make_unique<SB::RPI5::RP1PWM>(tracer);
        
        auto itPin = options.find("pin");
        bool bHasPin = (itPin != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}

        int pin = std::stoi(itPin->second);
        bool bok = PwmRegisters->clrInvert(pin);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
}

bool cmdFastClock(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdFastClock", tracer);
    try
    {
        auto itPin = options.find("pin");
        auto itPeriods = options.find("periods");
        bool bHasPin = (itPin != options.end());
        bool bHasPeriods = (itPeriods != options.end());

		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}

        if (!bHasPeriods)
        {
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the periods option"));
			return false;
        }

        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        int pin = std::stoi(itPin->second);
        int period = std::stoi(itPeriods->second);
        bool bok = GpioRegisters->FastClock(pin, period);
        if (!bok)
        {
            errors.emplace_back(std::format("RUNTIME ERROR - FastClock failed"));
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    return false;
}
bool cmdGetGpioPad(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetGpioStatus", tracer);
    try
    {
		auto it = options.find("pin");
        //auto it = options.find("retries");
        int temperature, humidity;
        
        bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);

        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t pad = GpioRegisters->getPad(pin);
        if (pad == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : cmdGetGpioPad({0}) failed", pin));
            return false;
        }
        logRegister(pad, std::format("PAD_IO{0} Status", pin));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    

}
bool cmdGetGpioStatus(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetGpioStatus", tracer);
    try
    {
		auto it = options.find("pin");
        //auto it = options.find("retries");
        int temperature, humidity;
        
        bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);

        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t status = GpioRegisters->getGpioStatus(pin);
        if (status == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : getGpioStatus({0}) failed", pin));
            return false;
        }
        logRegister(status, std::format("GPIO{0} Status", pin));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}
bool cmdGetGpioCntrl(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetGpioCntrl", tracer);
    try
    {
		auto it = options.find("pin");
        //auto it = options.find("retries");
        int temperature, humidity;
        
        bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);

        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t cntrl = GpioRegisters->getGpioCntrl(pin);
        if (cntrl == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : cmdGetGpioCntrl({0}) failed", pin));
            return false;
        }
        logRegister(cntrl, std::format("GPIO{0} Control", pin));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}
bool cmdGetRioOutEnabled(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetRioOutEnabled", tracer);
    try
    {
        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t rioOutEnable = GpioRegisters->getRioOutputEnable();
        if (rioOutEnable == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : cmdGetRioOutEnabled() failed"));
            return false;
        }
        logRegister(rioOutEnable, std::format("RIO_OE Control"));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}
bool cmdGetRioOut(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetRioOut", tracer);
    try
    {
        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t rioOut = GpioRegisters->getRioOut();
        if (rioOut == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : cmdGetRioOut() failed"));
            return false;
        }
        logRegister(rioOut, std::format("RIO_OUT Control"));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}
bool cmdGetRioIn(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetRioIn", tracer);
    try
    {
        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t rioIn = GpioRegisters->getRioIn();
        if (rioIn == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : cmdGetRioIn() failed"));
            return false;
        }
        logRegister(rioIn, std::format("RIO_IN Control"));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}
bool cmdGetRioInSync(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetRioInSync", tracer);
    try
    {
        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        uint32_t rioInSync = GpioRegisters->getRioInSync();
        if (rioInSync == static_cast<uint32_t>(-1))
        {
            errors.push_back(std::format("RUNTIME_ERROR : cmdGetRioInSync() failed"));
            return false;
        }
        logRegister(rioInSync, std::format("RIO_INSYNC Control"));
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}

bool cmdReadDHT11(const std::unordered_map<std::string,std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdReadDHT11", tracer);
    try
    {
		auto it = options.find("pin");
        //auto it = options.find("retries");
        int temperature, humidity;
        
        bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);
        SB::RPI5::Sensors::CDhct11 sensor(tracer, pin);

        bool bok = sensor.Read(temperature, humidity, errors);
        if (!bok)
        {
            errors.emplace_back("Read DHCT11 chip data failed");
            return false;
        }

        cout << "Temperature : " << temperature << " °C" << endl;
        cout << "humidity    : " << humidity << endl;

        trace.Info("Temperature : %ld °C", temperature);
        trace.Info("Humidity    : %ld", humidity);
        return true;
    }
    catch(const std::exception& e)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << e.what() << endl;
		cerr << FWhite;
    }
    return false;    
}

bool cmdGetInput(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdGetInput", tracer);
    try
    {
		auto it = options.find("pin");
		bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);
        std::string reason = "";
		int value = ioPin.GetInput(pin, errors);
		if (value < 0)
			errors.emplace_back(std::format("RUNTIME-ERROR : could not read the pin {0}", pin));
        else
            cout << "Pin " << pin << ": " << value << endl;
        return true;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    return false;
}
bool cmdIsPinFree(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
	CFuncTracer trace("cmdSetPinHigh", tracer);
    try
    {
		auto it = options.find("pin");
		bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);
        std::string reason = "";
		bool bok = ioPin.CheckPinOutputSanity(pin, reason, errors);
		if (!bok)
			errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} high", pin));

        cout << "reason : " << reason << endl;
        return bok;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    return false;
}

bool cmdSetPinHigh(const std::unordered_map<std::string, std::string>& options, std::unordered_set<std::string>& flags, std::vector<std::string>& errors)
{
	CFuncTracer trace("cmdSetPinHigh", tracer);
    try
    {
        bool bok = false;
        bool bUseRegistry = flags.find("registry") != flags.end();
		auto it = options.find("pin");
		bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);

        if (bUseRegistry == false)
        {
            if (pinNr != -1)
                ioPin.ReleasePin(pinNr, errors);
            pinNr = pin;

            bok = ioPin.SetHigh(pinNr, errors);
            if (!bok)
                errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} high", pin));
        }
        else
        {
            if (GpioRegisters == nullptr)
                GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

                bok = GpioRegisters->setGpioPin(pin, true);
            if (!bok)
                errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} high", pin));
        }
        return bok;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    return false;
}
bool cmdSetPinLow(const std::unordered_map<std::string, std::string>& options, std::unordered_set<std::string>& flags, std::vector<std::string>& errors)
{
	CFuncTracer trace("cmdSetPinLow", tracer);
    try
    {
        bool bok = false;
        bool bUseRegistry = flags.find("registry") != flags.end();
		auto it = options.find("pin");
		bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);

        if (bUseRegistry == false)
        {
            if (pinNr != -1)
                ioPin.ReleasePin(pinNr, errors);
            pinNr = pin;

            bok = ioPin.SetLow(pinNr, errors);
            if (!bok)
                errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} low", pin));
        }
        else
        {
            if (GpioRegisters == nullptr)
                GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

            bok = GpioRegisters->setGpioPin(pin, false);
            if (!bok)
                errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} low", pin));
        }
        return bok;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    return false;
}
bool cmdSetPulse(const std::unordered_map<std::string, std::string>& options, std::unordered_set<std::string>& flags, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdSetPulse", tracer);
    try
    {
        bool bok = false;
		auto itPin = options.find("pin");
        auto itWidth = options.find("width");
        auto itLeadTime = (options.find("leadtime"));
        auto itPostTime = (options.find("posttime"));
		bool bHasPin = (itPin != options.end());
        bool bHasWidth = (itWidth != options.end());
        bool bHasLeadTime = (itLeadTime != options.end());
        bool bHasPostTime = (itPostTime != options.end());
        bool bPositive = (flags.find("positive") != flags.end());
        bool bListen = (flags.find("listen") != flags.end());
        bool bRegistry = (flags.find("registry") != flags.end());
        int leadTime = 10;
        int postTime = 10;

		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}

        if (!bHasWidth)
        {
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the width option"));
			return false;
        }

        if (bHasLeadTime) leadTime = std::stoi(itLeadTime->second);
        if (bHasPostTime) postTime = std::stoi(itPostTime->second);

		int pin = std::stoi(itPin->second);
        int width = std::stoi(itWidth->second);

        if (bRegistry == false)
        {
            if (pinNr != -1)
                ioPin.ReleasePin(pinNr, errors);
            pinNr = pin;

            bok = ioPin.SetPulse(pinNr, width, leadTime, postTime, (bPositive)? SB::RPI5::PulseType::ePositive: SB::RPI5::PulseType::eNegative, bListen, errors);
            if (!bok)
                errors.emplace_back(std::format("RUNTIME-ERROR : could not generate pulse on pin {0}", pin));
        }
        else
        {
            if (GpioRegisters == nullptr)
                GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);
            bok = GpioRegisters->SetPulse(pin, width, leadTime, postTime, (bPositive)? SB::RPI5::eRp1IoPulseType::ePositive : SB::RPI5::eRp1IoPulseType::eNegative, bListen);
            if (!bok)
                errors.emplace_back(std::format("RUNTIME-ERROR : Could not generate pulse on pin {0}", pin));
        }
        return bok;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    return false;
}
bool cmdPinSetFunction(const std::unordered_map<std::string, std::string>& options, std::vector<std::string>& errors)
{
    CFuncTracer trace("cmdPinSetFunction", tracer);
    try
    {
        if (GpioRegisters == nullptr)
            GpioRegisters = std::make_unique<SB::RPI5::RP1IO>(tracer);

        auto itPin = options.find("pin");
        auto itPad = options.find("pad");
        auto itFunc = options.find("func");

        bool bHasPin = (itPin != options.end());
        bool bHasPad = (itPad != options.end());
        bool bHasFunc = (itFunc != options.end());

        if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
        if (!bHasPad)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pad option"));
			return false;
		}
        if (!bHasFunc)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the func option"));
			return false;
		}
        int pin = std::stoi(itPin->second);
        int pad = std::stoi(itPad->second);
        int func = std::stoi(itFunc->second);

        bool bok = GpioRegisters->setFunction(pin, func, pad);
        return bok;
    }
    catch(const std::exception& e)
    {
        trace.Error("Exception occurred : %s", e.what());
    }
    return false;
}



bool cmdMeasRC(const std::unordered_map<std::string, std::string>& options,
               std::vector<std::string>& errors)
{
	CFuncTracer trace("cmdSetPinLow", tracer);
    try
    {
		auto it = options.find("pin");
		bool bHasPin = (it != options.end());
		if (!bHasPin)
		{
			errors.emplace_back(std::format("SYNTAX-ERROR : should contain the pin option"));
			return false;
		}
		int pin = std::stoi(it->second);
        if (pinNr != -1)
            ioPin.ReleasePin(pinNr, errors);
		pinNr = pin;
        int time = ioPin.MeasRC(pinNr, errors);
		if (time < 0)
        {
			errors.emplace_back(std::format("RUNTIME-ERROR : could not measRC at pin {0} high", pin));
            return false;
        }

        cout << "RC time : " << time << " ms." << endl;
        trace.Info("time = %ld ms", time);
        return true;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    return false;
}

bool cmdEnumChips(std::vector<std::string> errors)
{
	CFuncTracer trace("cmdEnumChips", tracer);
	try
	{
		SB::RPI5::SBPio pio(tracer);

        auto chips = pio.enumChips(errors);
        for(auto chip : chips)
            cout << "chip: " << chip.name << ", label: " << chip.label << ", lines: " << chip.lines << endl;
        LogErrors(errors);
        return true;
	}
	catch(const std::exception& e)
	{
		trace.Error("Exception occurred : %s", e.what());
	}
	return false;
}

bool Shell()
{
    bool bStop = false;
    char cCmdLine[1024] = {0};

    try
    {
          using_history();

        // Make sure arrow keys work even if terminfo/keymaps are missing
        rl_bind_keyseq("\\e[A", rl_get_previous_history); // Up
        rl_bind_keyseq("\\e[B", rl_get_next_history);     // Down
        rl_bind_keyseq("\\e[C", rl_forward_char);         // Right (optional)
        rl_bind_keyseq("\\e[D", rl_backward_char);        // Left  (optional)

        std::vector<std::string> errors;


        do
        {
            std::cout << std::endl;
            char* line = readline("Command : ");
            if (!line)
                break;

            std::string sCmdLine(line);
            free(line);
            if (sCmdLine.empty())
                continue;

            // Add the in-memory history (enables up/down navigation)
            add_history(sCmdLine.c_str());

            // optional: persistent
            // write_history(".cli_history");

            auto vcArgs = string_ext::split(sCmdLine, ' ');

            if (!vcArgs.empty())
            {
                auto pars = MOW::Application::CLI::ParseLine(sCmdLine);

                errors.clear();
                eCmd cmd = GetCommand(vcArgs[0]);

                switch (cmd)
                {
                    case eCmd::eSetHigh:
                    {
                        bool ok = cmdSetPinHigh(pars.options, pars.flags, errors);
                        if (!ok)
                        {
                            errors.emplace_back("cmdSetPinHigh failed!");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eSetLow:
                    {
                        bool ok = cmdSetPinLow(pars.options, pars.flags, errors);
                        if (!ok)
                        {
                            errors.emplace_back("cmdSetPinLow failed!");
                            cerr << "cmdSetPinLow failed!" << endl;
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eGetInput:
                    {
                        bool ok = cmdGetInput(pars.options, errors);
                        if (!ok)
                        {
                            errors.emplace_back("cmdGetInput failed!");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eMeasRC:
                    {
                        bool ok = cmdMeasRC(pars.options, errors);
                        if (!ok)
                        {
                            errors.emplace_back("cmdMeasRC failed!");
                            cerr << "cmdMeasRC failed!" << endl;
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eSetPulse:
                    {
                        bool bok = cmdSetPulse(pars.options, pars.flags, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdSetPulse failed!");
                            cerr << "cmdSetPulsee failed" << endl;
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eIsPinFree:
                    {
                        bool ok = cmdIsPinFree(pars.options, errors);
                        if (!ok)
                        {
                            errors.emplace_back("cmdPinFree failed!");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::enumchips:
                    {
                        bool bok = cmdEnumChips(errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdEnumChips failed!");
                            Usage(errors);
                        }
                    }
                    break;

					case eCmd::testParameterParsing:
					{
						LogParameters(pars);
					}
					break;

                    case eCmd::readDHT11:
                    {
                        bool bok = cmdReadDHT11(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdReadDHT11 failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetGpioStatus:
                    {
                        bool bok = cmdGetGpioStatus(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetGpioStatus failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetGpioCntrl:
                    {
                        bool bok = cmdGetGpioCntrl(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetGpioCntrl failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetRpioOut:
                    {
                        bool bok = cmdGetRioOut(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetRioOut failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetRpioOE:
                    {
                        bool bok = cmdGetRioOutEnabled(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetRioOutEnabled failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetRpioIn:
                    {
                        bool bok = cmdGetRioIn(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetRioIn failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetRpioInSync:
                    {
                        bool bok = cmdGetRioInSync(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetRioInSync failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eGetPAD:
                    {
                        bool bok = cmdGetGpioPad(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdGetGpioPad failed");
                            Usage(errors);
                        }
                    }
                    break;
                    case eCmd::eFastClk:
                    {
                        bool bok = cmdFastClock(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdFastClock failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePinSetFunction:
                    {
                        bool bok = cmdPinSetFunction(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPinSetFunction failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eQuit:
					{
                        ioPin.ReleasePin(pinNr, errors);
                        bStop = true;
					}
                    break;

                    case eCmd::ePwmGetReg:
                    {
                        bool bok = cmdPwmGetRegisters(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmRegisters failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmGetClk:
                    {
                        bool bok = cmdPwmGetClockRegisters(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmGetClockRegisters failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmGetRegGlobal:
                    {
                        bool bok = cmdPwmGetGlobal(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmGetGlobal failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmSetFrequencyDuty:
                    {
                        bool bok = cmdPwmSetFrequency(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmSetFrequency failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmEnable:
                    {
                        bool bok = cmdPwmEnable(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmEnable failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmDisable:
                    {
                        bool bok = cmdPwmDisable(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmDisable failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmSetMode:
                    {
                        bool bok = cmdPwmSetMode(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmSetMode failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmSetInvert:
                    {
                        bool bok = cmdPwmSetInvert(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmSetInvert failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmClearInvert:
                    {
                        bool bok = cmdPwmClearInvert(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmClearInvert failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::ePwmSetRangeDutyPhase:
                    {
                        bool bok = cmdPwmSetRange(pars.options, errors);
                        if (!bok)
                        {
                            errors.emplace_back("cmdPwmSetRange failed");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eUnknown:
                    default:
                    {
                        errors.emplace_back(
                            std::format("Unknown command '{}'", vcArgs[0]));
                        cerr << "Unknown command '" << vcArgs[0] << "'" << endl;
                        Usage(errors);
                    }
                    break;
                }
            }
        } while (!bStop);

        return true;
    }
    catch (const std::exception& ex)
    {
		cerr << FRed;
        cerr << "ERROR - exception in cmdSetPinHigh: " << ex.what() << endl;
		cerr << FWhite;
    }
    catch (...)
    {
		cerr << FRed;
        cerr << "Unknown exception in Shell()" << endl;
		cerr << FWhite;
    }

    return false;
}

int main(int argc, char* argv[])
{
    std::vector<std::string> errors;

	CFuncTracer trace("main", tracer);

    auto pars = MOW::Application::CLI::Parse(
        argc, argv, MOW::Application::CLI::FlagMode::MultipleChars);

    m_Metrics.insert(std::make_pair("LogParametersTiming",
                                    MOW::Statistics::MetricValue()));

    LogParameters(pars);

    if (argc < 2)
    {
        errors.emplace_back(
            std::format("SYNTAX ERROR - wrong number of parameters ({})", argc));
        Usage(errors);
    }
    else
    {
        std::string sargCommand = argv[1];

        switch (GetCommand(sargCommand))
        {
            case eCmd::eShell:
                Shell();
                break;

            default:
                errors.emplace_back(
                    std::format("Unknown/Unsupported command : {}", argv[1]));
                Usage(errors);
                break;
        }
    }

    return 0;
}
