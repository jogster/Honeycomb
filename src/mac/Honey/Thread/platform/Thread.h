// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma once

/** \cond */
namespace honey
{

class Mutex;

namespace platform { class Thread; }

namespace thread
{
    namespace platform
    {
        /// Thread local store. Every thread has its own separate store, can be retrieved statically.
        struct LocalStore
        {
            typedef honey::platform::Thread Thread;
            
            /// Init for process
            static bool init();
            /// Create thread local store
            static LocalStore& create(Thread& thread);
            /// Destroy thread local store
            static void destroy();
            /// Get thread local store
            static LocalStore& inst();

            Thread* thread;

            static pthread_key_t _key;
        };
    }
    
    namespace current { namespace platform
    {
        inline void yield()                                     { pthread_yield_np(); }
        inline void pause()                                     {}
    } }
}

namespace platform
{

class Thread
{
    typedef thread::platform::LocalStore LocalStore;
    friend struct thread::platform::LocalStore;
public:
    Thread(bool external, int stackSize);
    Thread(Thread&& rhs);
    virtual ~Thread();

    Thread& operator=(Thread&& rhs);

    static Thread& current();

    void start();
    void join();

    static int priorityNormal();
    static int priorityMin();
    static int priorityMax();
    void setPriority(int priority);
    int getPriority() const;
    
    typedef int ThreadId;
    static const ThreadId threadIdInvalid                       = 0;
    ThreadId threadId() const                                   { return _id; }

    static int concurrency()                                    { static int ret = concurrency_priv(); return ret; }

private:
    void finalize();

    static Thread* createExt();
    static void* entry(void* arg);
    static int concurrency_priv();

    pthread_t   _handle;
    ThreadId    _id;
    int         _stackSize;
    UniquePtr<honey::Mutex> _lock;
};

} }
/** \endcond */