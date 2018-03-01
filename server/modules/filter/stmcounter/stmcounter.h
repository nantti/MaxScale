#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

// Using features that exist in the latest gcc-4 series, 4.4.7.

#include <maxscale/cppdefs.hh>
#include <maxscale/filter.hh>
#include "stmsession.h"

namespace stm_counter
{
class StmCounter : public maxscale::Filter<StmCounter, StmSession>
{
public:
    ~StmCounter();
    static StmCounter* create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER* pParams);

    StmSession* newSession(MXS_SESSION* pSession);

    void diagnostics(DCB* pDcb);
    json_t* diagnostics_json() const;

    uint64_t getCapabilities();

private:
    StmCounter();

    StmCounter(const StmCounter&);
    StmCounter& operator = (const StmCounter&);

private:
};
} // stm_counter
