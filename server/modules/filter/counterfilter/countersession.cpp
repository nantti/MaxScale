/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "counterfilter"

#include "countersession.h"
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

namespace stm_counter
{
CounterSession::CounterSession(MXS_SESSION* mxsSession)
    : maxscale::FilterSession(mxsSession)
{
    MXS_DEBUG("CounterSession::CounterSession\n");
}

CounterSession::~CounterSession()
{
    MXS_DEBUG("CounterSession::~CounterSession\n");
}

CounterSession* CounterSession::create(MXS_SESSION* mxsSession)
{
    MXS_DEBUG("CounterSession::create\n");
    return new CounterSession(mxsSession);
}

int CounterSession::routeQuery(GWBUF *buffer)
{
    MXS_DEBUG("Stm is sql   %d\n", modutil_is_SQL(buffer));
    MXS_DEBUG("Stm prep stm %d\n", modutil_is_SQL_prepare(buffer));
    MXS_DEBUG("Stm num stms %d\n", modutil_count_statements(buffer));

    // prepared statements?
    // multiple statements?

    std::string sql = to_sql(buffer);
    MXS_DEBUG("Stm %s\n", sql.c_str());

    return mxs::FilterSession::routeQuery(buffer);
}

int64_t CounterSession::sessionId()
{
    return m_pSession->ses_id;
}
} // stm_counter
