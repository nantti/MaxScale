/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "stmcounter"

#include "stmsession.hh"
#include <maxscale/modutil.h>
#include <string>

namespace
{
// this probably exists already, maybe all of modutil should
// be available in c++ as well.
std::string to_sql(GWBUF *buffer)
{
    char *sql;
    int len;

    return modutil_extract_SQL(buffer, &sql, &len) ?
           std::string(sql, sql + len) : std::string();
}
} // anonymous

namespace maxscale
{
StmSession::StmSession(MXS_SESSION* pSession, const StmCounter* pFilter)
    : maxscale::FilterSession(pSession)
    , m_filter(*pFilter)
{
    MXS_DEBUG("StmSession::StmSession\n");
}

StmSession::~StmSession()
{
    MXS_DEBUG("StmSession::~StmSession\n");
}

StmSession* StmSession::create(MXS_SESSION* pSession, const StmCounter* pFilter)
{
    MXS_DEBUG("StmSession::create\n");
    return new StmSession(pSession, pFilter);
}

int StmSession::routeQuery(GWBUF *buffer)
{
    MXS_DEBUG("StmSession::routeQuery\n");

    // prepared statemens?
    // multiple statements?

    MXS_DEBUG("Stm is sql   %d\n", modutil_is_SQL(buffer));
    MXS_DEBUG("Stm prep stm %d\n", modutil_count_statements(buffer));
    MXS_DEBUG("Stm num stms %d\n", modutil_is_SQL_prepare(buffer));

    MXS_DEBUG("Stm %s\n", to_sql(buffer).c_str());


    return mxs::FilterSession::routeQuery(buffer);
}
}
