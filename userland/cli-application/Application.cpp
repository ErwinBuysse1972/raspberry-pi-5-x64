#include "Helpers/CLIParameters.h"
#include "Helpers/Metrics.h"
#include "Helpers/string_ext.h"
#include "Helpers/SBPio.h"
#include "Helpers/cout_ext.h"
#include "Helpers/DHT11.h"
#include "Helpers/SBRp1IO.h"
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

using std::cout;
using std::cerr;
using std::endl;

std::unordered_map<std::string, MOW::Statistics::MetricValue> m_Metrics;
std::shared_ptr<CFileTracer> tracer = std::make_shared<CFileTracer>("./", "cliApplication.log", TracerLevel::TRACER_DEBUG_LEVEL);
std::unique_ptr<SB::RPI5::RP1IO> GpioRegisters = nullptr;
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

    eGetGpioStatus,
    eGetGpioCntrl,
    eGetRpioOut,
    eGetRpioOE,
    eGetRpioIn,
    eGetRpioInSync,
    eGetPAD,

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
    cout << "options:" << endl;
    cout << "    --pin=<number> : give the pin you want to perform the actions" << endl;
    cout << "    --width=time   : set the width time in us." << endl;
    cout << "    --leadtime=time : is the time that the level is initial set before the pulse is generated" << endl;
    cout << "    --posttime=time : is the time that the level is kept after the pulse is generated" << endl;
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

bool cmdGetInput(const std::unordered_map<std::string, std::string>& options, 
                std::vector<std::string>& errors)
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
bool cmdIsPinFree(const std::unordered_map<std::string, std::string>& options,
                   std::vector<std::string>& errors)
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

bool cmdSetPinHigh(const std::unordered_map<std::string, std::string>& options,
                   std::vector<std::string>& errors)
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

        if (pinNr != -1)
            ioPin.ReleasePin(pinNr, errors);
        pinNr = pin;

		bool bok = ioPin.SetHigh(pinNr, errors);
		if (!bok)
			errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} high", pin));
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
        if (pinNr != -1)
            ioPin.ReleasePin(pinNr, errors);
        pinNr = pin;

		bool bok = ioPin.SetPulse(pinNr, width, leadTime, postTime, (bPositive)? SB::RPI5::PulseType::ePositive: SB::RPI5::PulseType::eNegative, bListen, errors);
		if (!bok)
			errors.emplace_back(std::format("RUNTIME-ERROR : could not generate pulse on pin {0}", pin));
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

bool cmdSetPinLow(const std::unordered_map<std::string, std::string>& options,
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

		bool bok = ioPin.SetLow(pinNr, errors);
		if (!bok)
			errors.emplace_back(std::format("RUNTIME-ERROR : could not set the pin {0} low", pin));
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
        std::vector<std::string> errors;

        do
        {
            std::cout << std::endl;
            std::cout << "Command : ";
            std::cin.getline(cCmdLine, 1023);

            std::string sCmdLine = cCmdLine;
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
                        bool ok = cmdSetPinHigh(pars.options, errors);
                        if (!ok)
                        {
                            errors.emplace_back("cmdSetPinHigh failed!");
                            Usage(errors);
                        }
                    }
                    break;

                    case eCmd::eSetLow:
                    {
                        bool ok = cmdSetPinLow(pars.options, errors);
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

                    case eCmd::eQuit:
					{
                        ioPin.ReleasePin(pinNr, errors);
                        bStop = true;
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
