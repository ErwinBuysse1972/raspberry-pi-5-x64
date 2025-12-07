#pragma once
#include <iostream>

#ifdef _WIN32
    #include <Windows.h>

    inline std::ostream& FBlack(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, 0);
        return s;
    }
    inline std::ostream& FBlue(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_BLUE);
        return s;
    }
    inline std::ostream& FRed(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_RED);
        return s;
    }
    inline std::ostream& FGreen(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_GREEN);
        return s;
    }
    inline std::ostream& FYellow(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_GREEN | FOREGROUND_RED);
        return s;
    }
    inline std::ostream& FWhite(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN);
        return s;
    }

    inline std::ostream& BWhite(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color |= (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED);
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }
    inline std::ostream& BBlue(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color |= BACKGROUND_BLUE;
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }
    inline std::ostream& BRed(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color |= BACKGROUND_RED;
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }

    inline std::ostream& Bold_on(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color |= FOREGROUND_INTENSITY;
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }
    inline std::ostream& Bold_off(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color &= ~FOREGROUND_INTENSITY;
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }

    inline std::ostream& Underline_on(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color |= COMMON_LVB_UNDERSCORE;
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }
    inline std::ostream& Underline_off(std::ostream& s)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hstdout, &info);
        WORD color = info.wAttributes;
        color &= ~COMMON_LVB_UNDERSCORE;
        ::SetConsoleTextAttribute(hstdout, color);
        return s;
    }

    struct CoutColor
    {
        explicit CoutColor(WORD attrib)
            : m_color(attrib)
        {}
        WORD m_color;
    };

    template <class _Elem, class _Traits>
    std::basic_ostream<_Elem, _Traits>& operator<<(std::basic_ostream<_Elem, _Traits>& i, const CoutColor& c)
    {
        HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ::SetConsoleTextAttribute(hstdout, c.m_color);
        return i;
    }

#else  // Linux / POSIX – use ANSI escape codes

    // Helper to write ANSI SGR code
    inline std::ostream& ansi(std::ostream& s, const char* code)
    {
        return s << "\033[" << code << "m";
    }

    inline std::ostream& FBlack(std::ostream& s)   { return ansi(s, "30"); }
    inline std::ostream& FRed(std::ostream& s)     { return ansi(s, "31"); }
    inline std::ostream& FGreen(std::ostream& s)   { return ansi(s, "32"); }
    inline std::ostream& FYellow(std::ostream& s)  { return ansi(s, "33"); }
    inline std::ostream& FBlue(std::ostream& s)    { return ansi(s, "34"); }
    inline std::ostream& FWhite(std::ostream& s)   { return ansi(s, "37"); }

    inline std::ostream& BRed(std::ostream& s)     { return ansi(s, "41"); }
    inline std::ostream& BBlue(std::ostream& s)    { return ansi(s, "44"); }
    inline std::ostream& BWhite(std::ostream& s)   { return ansi(s, "47"); }

    inline std::ostream& Bold_on(std::ostream& s)        { return ansi(s, "1"); }
    inline std::ostream& Bold_off(std::ostream& s)       { return ansi(s, "22"); }
    inline std::ostream& Underline_on(std::ostream& s)   { return ansi(s, "4"); }
    inline std::ostream& Underline_off(std::ostream& s)  { return ansi(s, "24"); }

    // Optional: reset everything (colors + bold/underline)
    inline std::ostream& Reset(std::ostream& s)
    {
        return ansi(s, "0");
    }

    struct CoutColor
    {
        explicit CoutColor(const char* code)
            : m_code(code)
        {}
        const char* m_code;
    };

    inline std::ostream& operator<<(std::ostream& os, const CoutColor& c)
    {
        return ansi(os, c.m_code);
    }

#endif
