#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

// Using features that exist in the latest gcc-4 series, 4.4.7.

#include <maxscale/filter.hh>
#include <maxscale/spinlock.hh>
#include "countersession.h"
#include "eventstats.h"
#include "eventout.h"
#include "stopwatch.h"
#include <iostream>
#include <thread>
#include <memory>

namespace stm_counter
{
struct Config
{
    enum FileType {Text, Json};

    base::Duration           timeWindow;
    std::vector<std::string> eventFilter;
    FileType                 fileType;
    std::ios_base::openmode  fileMode;
    std::string              reportFile;
    std::string              totalsFile;
};

// Filter to keep statistics of sql statement usage.
// This is an example config:
//  [Statement-Counter]
//  type=filter
//  module=counterfilter
//  # time window in seconds. Default 60.
//  time-window=10
//  # Count these events (will be upper-cased). "*" - count everything.
//  # default *
//  events=select,insert,update,delete
//  # text or json. Appends extension (txt,json), unless already there.
//  # default json
//  file-type=json
//  # create or append. Default create.
//  file-mode=create
//  # report file path, directory must exist.
//  report-file=/tmp/counter-report
//  # totals report file path, directory must exist.
//  totals-file=/tmp/counter-totals

class CounterFilter : public maxscale::Filter<CounterFilter, CounterSession>
{
public:
    ~CounterFilter();
    static CounterFilter* create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER* pParams);

    CounterSession* newSession(MXS_SESSION* mxsSession);

    void diagnostics(DCB* pDcb);
    json_t* diagnostics_json() const;
    uint64_t getCapabilities();
    const Config& config() const {return _config;}
    void sessionClose(CounterSession* session);
    void increment(SessionStats* stats, const std::string& event);
private:
    CounterFilter(MXS_CONFIG_PARAMETER* params);
    CounterFilter(const CounterFilter&);
    CounterFilter& operator = (const CounterFilter&);

    Config _config;
    std::vector<SessionData> _sessionData;

    void _fillReport(std::ostream& os, Config::FileType type);
    void _fillTotalsReport(std::ostream& os, Config::FileType type);
    void _report();
    void _purge();
    std::string _addExtension(const std::string& fileName);
    std::thread _notifier;
    void _notifier_run();
    bool _shutdown;
    mxs::SpinLock _sessionLock;
};
} // stm_counter
