#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <regex>

namespace MOW::Application::CLI
{
    struct ArgumentParseResult
    {
        std::string command;
        std::unordered_set<std::string> flags;
        std::unordered_map<std::string, std::string> options;
        std::vector<std::string> positionals;
        std::vector<std::string> errors;
    };

    // Hoe flags ge�nterpreteerd worden
    enum class FlagMode
    {
        WholeToken,     // "-FlagA" => "FlagA"
        MultipleChars   // "-abc" => "a", "b", "c"
    };

    inline std::string to_lower_ascii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    inline ArgumentParseResult Parse(const std::vector<std::string>& args, FlagMode flagMode = FlagMode::WholeToken)
    {
        ArgumentParseResult res;

        if (args.empty())
        {
            res.errors.emplace_back("No arguments provided.");
            return res;
        }

        bool stopParsing = false;

        for (std::size_t i = 0; i < args.size(); ++i)
        {
            std::string token = args[i];

            if (stopParsing)
            {
                res.positionals.push_back(token);
                continue;
            }

            if (token == "--")
            {
                stopParsing = true;
                continue;
            }

            // Long option: --key=value
            if (token.rfind("--", 0) == 0) // starts with "--"
            {
                auto eqPos = token.find('=');
                if (eqPos == std::string::npos)
                {
                    res.errors.emplace_back("Options requires '=value':" + token);
                    continue;
                }

                std::string key = token.substr(2, eqPos - 2);
                std::string value = token.substr(eqPos + 1);

                if (key.empty())
                {
                    res.errors.emplace_back("Empty option name in token: " + token);
                    continue;
                }

                res.options[to_lower_ascii(key)] = value;
                continue;
            }

            // Flags: -Something  (interpretatie hangt af van FlagMode)
            if (token.size() > 1 && token[0] == '-')
            {
                // In jouw C#-code wordt er ook gecheckt op extra '-'
                bool malformed = false;
                for (std::size_t k = 1; k < token.size(); ++k)
                {
                    if (token[k] == '-')
                    {
                        res.errors.emplace_back("Malformed flag token: " + token);
                        malformed = true;
                        break;
                    }
                }

                if (malformed)
                    continue;

                if (flagMode == FlagMode::WholeToken)
                {
                    // C#-stijl: volledige substring na de eerste '-' als ��n flag
                    // "-GeenZever" => "GeenZever"
                    std::string flag = token.substr(1);
                    res.flags.insert(to_lower_ascii(flag));
                }
                else // FlagMode::MultipleChars
                {
                    // Unix-stijl: "-abc" => "a","b","c"
                    for (std::size_t k = 1; k < token.size(); ++k)
                    {
                        char c = token[k];
                        // (extra '-' is al eerder afgecheckt)
                        std::string flag(1, c);
                        res.flags.insert(to_lower_ascii(flag));
                    }
                }

                continue;
            }

            // Command + positionals
            if (res.command.empty())
            {
                res.command = token;
            }
            else
            {
                res.positionals.push_back(token);
            }
        }

        if (res.command.empty())
        {
            res.errors.emplace_back("No command provided.");
        }

        return res;
    }

    // Convenience overload voor argc/argv
    inline ArgumentParseResult Parse(int argc, char* argv[],
        FlagMode flagMode = FlagMode::WholeToken)
    {
        std::vector<std::string> args;

        // argv[0] is meestal programmanaam; in C# zit die niet in args[],
        // dus we starten vanaf 1.
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i])
                args.emplace_back(argv[i]);
            else
                args.emplace_back("");
        }

        return Parse(args, flagMode);
    }

    // Helper: replace all occurrences of `from` with `to` in-place
    inline void replace_all(std::string& s,
                            const std::string& from,
                            const std::string& to)
    {
        if (from.empty())
            return;

        std::size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos)
        {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    // C# Regex:
    // (?<k>[^\s""=]+)=""(?<v>(?:""""|[^""])*)""|""(?<q>(?:""""|[^""])*)""|(?<w>[^\s""]+)
    //
    // C++ version with numbered groups:
    //  1: k
    //  2: v
    //  3: q
    //  4: w
    inline std::vector<std::string> TokenizeCommandLine(const std::string& line)
    {
        std::vector<std::string> args;

        // Using a raw string literal with a custom delimiter to make the quotes manageable
        static const std::regex re(
            R"REGEX(([^ \t"=]+)="((?:""|[^"])*)"|"((?:""|[^"])*)"|([^ \t"]+))REGEX",
            std::regex::ECMAScript
        );

        std::sregex_iterator it(line.begin(), line.end(), re);
        std::sregex_iterator end;

        for (; it != end; ++it)
        {
            const std::smatch& m = *it;

            std::string token;

            if (m[1].matched)  // k="v"
            {
                std::string key   = m[1].str();
                std::string value = m[2].str();  // still has "" as escaped quotes

                // value.Replace(@"""""", @"""") in C#
                replace_all(value, R"("")", R"(")");

                token = key + "=" + value;
            }
            else if (m[3].matched)  // "q"
            {
                std::string value = m[3].str();
                replace_all(value, R"("")", R"(")");
                token = value;
            }
            else if (m[4].matched)  // w
            {
                token = m[4].str();
            }
            else
            {
                // Should not happen, but just in case
                continue;
            }

            args.push_back(std::move(token));
        }

        return args;
    }

    // Convenience function that fully mimics the C# snippet
    inline ArgumentParseResult ParseLine(const std::string& line, FlagMode flagMode = FlagMode::WholeToken)
    {
        auto args = TokenizeCommandLine(line);
        return Parse(args, flagMode);
    }


} // namespace MOW::Application::CLI
