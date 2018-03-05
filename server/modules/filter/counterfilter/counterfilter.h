#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

// Using features that exist in the latest gcc-4 series, 4.4.7.

#include <maxscale/filter.hh>
#include "countersession.h"
#include "statementstats.h"
#include "stopwatch.h"

namespace stm_counter
{
struct Config
{
    enum FileType {Text, Json};
    enum FileMode {Create, Append};

    base::Duration timeWindow;
    std::vector<std::string> events;
    FileType    fileType;
    FileMode    fileMode;
    std::string reportFile;
    std::string totalsFile;
};

class CounterFilter : public maxscale::Filter<CounterFilter, CounterSession>
{
public:
    ~CounterFilter();
    static CounterFilter* create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER* pParams);

    CounterSession* newSession(MXS_SESSION* mxsSession);
    void closeSession(MXS_FILTER *mxsSession, MXS_FILTER_SESSION *session);

    void diagnostics(DCB* pDcb);
    json_t* diagnostics_json() const;
    uint64_t getCapabilities();
    const Config& config() const {return _config;}

    void statisticsChanged(CounterSession* session); // in lieu of timer threads
private:
    CounterFilter(MXS_CONFIG_PARAMETER* params);
    CounterFilter(const CounterFilter&);
    CounterFilter& operator = (const CounterFilter&);

    Config _config;
    std::vector<SessionData> _sessionData;
    base::TimePoint _nextReportTime;

    void _fillReport(std::ostream& os);
    void _fillTotalsReport(std::ostream& os);
    void _report();
    void _purge();
};
} // stm_counter
