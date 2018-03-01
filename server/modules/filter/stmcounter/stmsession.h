#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#include <maxscale/cppdefs.hh>
#include <maxscale/filter.hh>


namespace stm_counter
{

class StmCounter;

class StmSession : public maxscale::FilterSession
{
public:
    static StmSession* create(MXS_SESSION* pSession, const StmCounter* pFilter);
    int routeQuery(GWBUF* buffer);

    ~StmSession();
private:
    StmSession(MXS_SESSION* pSession, const StmCounter* pFilter);

    StmSession(const StmSession&);
    StmSession& operator = (const StmSession&);

private:
    const StmCounter& m_filter;
};
} // stm_counter
