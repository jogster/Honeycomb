// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma hdrstop

#include "Honey/App/App.h"
#include "Honey/App/Module.h"
#include "Honey/Misc/Log.h"

namespace honey
{

App::App() :
    _interruptFreq(30),
    _thread(nullptr),
    _runMode(RunMode::term) {}

App::~App() {}
    
void App::entry()
{
    {
        Mutex::Scoped _(_lock);
        _thread = &Thread::current();
    }
    
    _runMode = RunMode::run;
    run();
    
    {
        Mutex::Scoped _(_lock);
        _thread = nullptr;
    }
}

void App::interrupt(const Exception::ConstPtr& e)
{
    Mutex::Scoped _(_lock);
    if (!_thread) return;
    _thread->interrupt(e);
}

void App::run()
{
    for (auto& e: values(app::ModuleRegistry::inst().modules)) DepSched::inst().reg(*e->task);
    DepSched::inst().enqueue(*app::ModuleRegistry::inst().modules["root"_id]->task);
    
    vector<app::Module::Ptr> modules;
    vector<Future<void>> results;
    for (auto& e: values(app::ModuleRegistry::inst().modules))
    {
        modules.push_back(e);
        results.push_back(move(e->task->future()));
    }
    
    while (!modules.empty())
    {
        try
        {
            thread::current::interruptPoint();
            auto it = future::waitAny(results, _runMode == RunMode::run ? Millisec::max() : 1000_ms / _interruptFreq);
            if (it != results.end())
            {
                try { it->get(); }
                catch (Terminated&) {}
                catch (std::exception& e)
                {
                    Log::inst() << log::level::critical << e.what();
                }
                modules.erase(modules.begin() + (it - results.begin()));
                results.erase(it);
            }
            switch (_runMode)
            {
            case RunMode::term:
                {
                    //interrupt remaining modules with terminate
                    for (auto& e: modules) e->task->interrupt(new Terminated());
                    break;
                }
            default: break;
            }
        }
        catch (Terminated&)
        {
            if (_runMode != RunMode::term)
            {
                Log::inst() << log::level::info << "terminating...";
                _runMode = RunMode::term;
            }
        }
    }
}

}
