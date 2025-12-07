#pragma once
#include <memory>
#include <string>
#include <cstring>
#include <chrono>
#include <ctracer.h>


#define MEASURE_FUNCTION(tracer) ScopeTimer(__func__, tracer)
#define CFUNCTRACER(tracer) CFuncTrace trace(__func__, tracer)
#define CFUNCTRACER_SEPARATOR   ((char *)"() : ")
#define TRACER_MAX_BUFFER_SIZE          1024

class CFuncTracer
{
private:
   std::string _functionName;
   std::shared_ptr<CTracer> _tracer;
   bool m_bStackTrace;
   bool m_bUseBinDataWriting;

   char TraceGetAsciiChar(unsigned char byte);

   void trace(const std::string& message);
   void info(const std::string& message);
   void warning(const std::string& message);
   void error(const std::string& message);
   void fatalError(const std::string& message);

public:
      CFuncTracer(const char *functionName, std::shared_ptr<CTracer> tracer, bool bStackTrace = true, bool bUseWriteBinData = false):
     _functionName(functionName),
     _tracer(tracer),
     m_bStackTrace(bStackTrace),
     m_bUseBinDataWriting(bUseWriteBinData)
     {
         if (m_bStackTrace)
            Trace("Entr");
     };
   ~CFuncTracer()
   {
       if (m_bStackTrace)
           Trace("Exit");
   };

   void Trace(const char* fmt, ...);
   void Info(const char* fmt, ...);
   void Warning(const char* fmt, ...);
   void Error(const char* fmt, ...);
   void FatalError(const char* fmt, ...);

   void LogDataBuffer(unsigned char *lpData, unsigned long Length, const char *logName);
};
