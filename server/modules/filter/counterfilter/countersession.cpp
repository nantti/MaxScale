/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "counterfilter"

#include "countersession.h"
#include "counterfilter.h"
#include "statementstats.h"
#include <maxscale/modutil.h>
#include <maxscale/query_classifier.h>
#include <string>
#include <algorithm>
#include <sstream>

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
CounterSession::CounterSession(MXS_SESSION* mxsSession, CounterFilter *filter, SessionStats *stats)
    : maxscale::FilterSession(mxsSession), _filter(filter), _stats(stats)
{
    MXS_DEBUG("CounterSession::CounterSession\n");
}

CounterSession::~CounterSession()
{
    MXS_DEBUG("CounterSession::~CounterSession\n");
}

int CounterSession::routeQuery(GWBUF *buffer)
{
    if (!modutil_is_SQL(buffer))
    {
        MXS_DEBUG("Not sql, ignore\n");
    }

    std::string sql = to_sql(buffer);
    MXS_DEBUG("Stm %s\n", sql.c_str());

    // I meant to report subqueries, but support would need to be added
    // to query_classifier (rather than parsing here). It would be enough
    // to get the expression tree and then use that here. It could be as
    // smart as you want, e.g. "alter table", rather than just alter.
    // Could avoid counting if the query is either incorrect, or the server
    // returned an error. On the other hand, why not count even if the
    // statement does not exist... (like "selct", misspelled).

    std::istringstream is(sql);
    std::string op;
    is >> op;
    std::transform(op.begin(), op.end(), op.begin(), ::toupper);
    _stats->increment(op);
    _filter->statisticsChanged(this);

    return mxs::FilterSession::routeQuery(buffer);
}

int64_t CounterSession::sessionId()
{
    return m_pSession->ses_id;
}
} // stm_counter
