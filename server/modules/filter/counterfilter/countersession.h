#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#include <maxscale/filter.hh>
#include "stopwatch.h"

// The CounterSession updates statistics, the CounterFilter does the reporting.
namespace stm_counter
{
class CounterFilter;
class SessionStats;

class CounterSession : public maxscale::FilterSession
{
public:
    CounterSession(MXS_SESSION* pSession, CounterFilter * filter, SessionStats* stats);
    CounterSession(const CounterSession&) = delete;
    CounterSession& operator = (const CounterSession&)  = delete;

    int routeQuery(GWBUF* buffer);
    int64_t sessionId();

    void setFilter(CounterFilter* filter);
    void setSessionStats(SessionStats* stats);

    ~CounterSession();
private:
    CounterFilter * _filter;
    SessionStats* _stats;
};
} // stm_counter
