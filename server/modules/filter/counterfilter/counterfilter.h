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

namespace stm_counter
{
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
private:
    CounterFilter();
    CounterFilter(const CounterFilter&);
    CounterFilter& operator = (const CounterFilter&);

    std::vector<SessionData> _sessionData;
};
} // stm_counter
