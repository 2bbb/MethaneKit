/******************************************************************************

Copyright 2020 Evgeny Gorodetskiy

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*******************************************************************************

FILE: Methane/Data/ScopeTimer.h
Code scope measurement timer with aggregating and averaging of timings.

******************************************************************************/

#pragma once

#include "Timer.hpp"
#include "ILogger.h"

#include <string>
#include <map>

namespace Methane::Data
{

class ScopeTimer : protected Timer
{
public:
    class Aggregator
    {
    public:
        struct Timing
        {
            TimeDuration duration;
            uint32_t     count    = 0u;
        };

        static Aggregator& Get();
        ~Aggregator();

        void SetLogger(ILogger::Ptr sp_logger)   { m_sp_logger = std::move(sp_logger); }
        const ILogger::Ptr& GetLogger() const { return m_sp_logger; }

        void AddScopeTiming(const std::string& scope_name, TimeDuration duration);
        const Timing& GetScopeTiming(const std::string& scope_name) const;

        void LogTimings(ILogger& logger);

    private:
        Aggregator() = default;

        using ScopeTimings = std::map<std::string, Timing>;

        ScopeTimings m_timing_by_scope_name;
        ILogger::Ptr m_sp_logger;
    };

    template<typename TLogger>
    static void InitializeLogger()
    {
        Aggregator::Get().SetLogger(std::make_shared<TLogger>());
    }

    ScopeTimer(std::string scope_name);
    ~ScopeTimer();

    const std::string& GetScopeName() const { return m_scope_name; }

private:
    std::string m_scope_name;
};

#ifdef SCOPE_TIMERS_ENABLED

#define SCOPE_TIMER(SCOPE_NAME) Methane::Data::ScopeTimer scope_timer(SCOPE_NAME)
#define FUNCTION_SCOPE_TIMER() SCOPE_TIMER(__func__)

#else

#define SCOPE_TIMER(SCOPE_NAME)
#define FUNCTION_SCOPE_TIMER()

#endif

} // namespace Methane::Data
