#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#include <maxscale/cppdefs.hh>
#include <maxscale/filter.hh>
#include "stmsession.hh"

namespace maxscale
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
}
