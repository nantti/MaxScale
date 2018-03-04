#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

//#include <maxscale/cppdefs.hh>
#include <maxscale/filter.hh>


namespace stm_counter
{
class CounterFilter;
class SessionStats;

class CounterSession : public maxscale::FilterSession
{
public:
    CounterSession(const CounterSession&) = delete;
    CounterSession& operator = (const CounterSession&)  = delete;;

    static CounterSession* create(MXS_SESSION* mxsSession);
    int routeQuery(GWBUF* buffer);
    int64_t sessionId();

    void setSessionStats(SessionStats* stats) {_stats = stats;}

    ~CounterSession();
private:
    CounterSession(MXS_SESSION* pSession);
    SessionStats* _stats;
};
} // stm_counter
