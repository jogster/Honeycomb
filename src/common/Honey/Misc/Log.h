// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma once

#include "Honey/Graph/Dep.h"
#include "Honey/Thread/Lock/Spin.h"

namespace honey
{

/// Open a debug log record with prepended source file info
#define Log_debug               Log::inst() << log::level::debug << "[" << log::srcFilename(__FILE__) << ":" << __LINE__ << "] "

/// Logger methods
namespace log
{
    /// Severity level
    typedef DepNode<void*, NameId> Level;
    
    /// Severity levels (default levels)
    namespace level
    {
        extern Level critical;  ///< Information describing a critical problem that has occurred
        extern Level error;     ///< Information describing a major problem that has occurred
        extern Level warning;   ///< Information describing a minor problem that has occurred
        extern Level info;      ///< General information
        extern Level debug;     ///< Low-level information for debugging purposes
    }
    
    /// Format record with date and level id
    String format(const Level& level, const String& record);
    
    struct Sink : SharedObj<Sink>
    {
        typedef SharedPtr<Sink> Ptr;
        
        virtual void operator()(const Level& level, const String& record) = 0;
    };
    
    /// Captures records in a buffer
    struct BufferSink : Sink
    {
        typedef SharedPtr<BufferSink> Ptr;
        
        virtual void operator()(const Level& level, const String& record);
        vector<tuple<const Level*, String>> records;
    };
    
    /// Formats record to a stream
    struct StreamSink : Sink
    {
        typedef SharedPtr<StreamSink> Ptr;
        
        StreamSink(ostream& os)                 : os(os) {}
        virtual void operator()(const Level& level, const String& record);
        ostream& os;
    };
    
    /// Formats record to a file stream
    struct FileSink : StreamSink
    {
        typedef SharedPtr<FileSink> Ptr;
        
        FileSink(String filepath);
        ~FileSink();
        virtual void operator()(const Level& level, const String& record);
        String filepath;
        std::ofstream os;
    };
    
    /// Get filename from source path provided by macro __FILE__
    inline String srcFilename(const String& path)       { szt pos = path.find_last_of(String("\\/")); return pos != String::npos ? path.substr(pos+1) : path; }
}

/// Logger
class Log
{
public:
    typedef DepGraph<const log::Level> LevelGraph;
    typedef unordered_map<Id, log::Sink::Ptr> SinkMap;
    
    /// Builds a record
    struct RecordStream : ostringstream
    {
        RecordStream(Log& log, const log::Level& level) : log(&log), lock(log._lock) { log._level = &level; }
        RecordStream(RecordStream&& rhs)                : ostringstream(std::move(rhs)), log(rhs.log), lock(std::move(rhs.lock)) { rhs.log = nullptr; }
        ~RecordStream()                                 { if (!log) return; log->push(str()); }
        Log* log;
        SpinLock::Scoped lock;
    };
    
    /// Get singleton
    static mt_global(Log, inst,);

    /// Create logger with default levels and a standard streams sinks ("stdout" and "stderr")
    Log();
    
    /// Add a severity level to categorize records
    void addLevel(const log::Level& level);
    void removeLevel(const log::Level& level);
    const LevelGraph& levels() const                    { return _levelGraph; }
    
    /// Add a sink to receive records
    void addSink(const Id& name, const log::Sink::Ptr& sink);
    void removeSink(const Id& name);
    const SinkMap& sinks() const                        { return _sinks; }
    
    /// Add a record filter to a sink
    /**
      * \param sink         sink to filter
      * \param includes     levels to push to sink
      * \param includeDeps  also include any levels that the includes depend on
      * \param excludes     levels to not push to sink
      * \param excludeDeps  also exclude any levels that the excludes depend on
      */  
    void filter(const Id& sink, const vector<const log::Level*>& includes, bool includeDeps = true,
                const vector<const log::Level*>& excludes = {}, bool excludeDeps = true);
    void clearFilter(const Id& sink);
    
    /// Push a record with level to all sinks
    RecordStream operator<<(const log::Level& level)    { return RecordStream(*this, level); }
    
    /// Acquire lock to synchronize output to sinks, other loggers should call this before outputting to the same sinks
    SpinLock::Scoped lock()                             { return _lock; }
    
private:
    typedef unordered_map<Id, std::set<Id>> FilterMap;

    void push(const String& record);
    
    LevelGraph _levelGraph;
    const log::Level* _level;
    SinkMap _sinks;
    FilterMap _filters;
    SpinLock _lock;
};

}
