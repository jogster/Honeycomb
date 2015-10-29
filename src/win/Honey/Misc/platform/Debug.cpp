// Honeycomb, Copyright (C) 2013 Daniel Carter.  Distributed under the Boost Software License v1.0.
#pragma hdrstop

#include "Honey/Misc/Debug.h"
#include "Honey/Misc/Exception.h"
#include "Honey/Misc/Log.h"

/** \cond */
namespace honey { namespace debug { namespace platform
{
void print(const String& str)
{
    auto _ = Log::inst().lock();
    std::cout << str;
    std::cout.flush();
    OutputDebugStringW(std::wstring(str.begin(), str.end()).c_str());
}

void print(const char* str) { print(String(str)); }

void assertFail(const char* expr, const char* func, const char* file, int line, const String& msg)
{
    #ifdef DEBUG
        String assert_ = sout() << expr << "\nMessage: " << msg;
        String file_ = file;
        _wassert(   std::wstring(assert_.begin(), assert_.end()).c_str(),
                    std::wstring(file_.begin(), file_.end()).c_str(), line);
    #endif
    String assert = sout()
        << "ASSERTION FAILED: " << expr << endl
        << (msg.length() ? String(sout() << msg << endl) : "");
    #ifndef FINAL
        print(sout() << assert
            << "Function: " << func << endl
            << "File: " << file << ":" << line << endl);
    #endif
    Exception::Raiser() ^ Exception::Source(func, file, line) << AssertionFailure() << assert;
}

void assertFail(const char* expr, const char* func, const char* file, int line, const char* msg)
{
    assertFail(expr, func, file, line, String(msg));
}

} } }
/** \endcond */



