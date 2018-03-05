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
    // smart as one wants, e.g. "alter table", rather than just alter.

    std::istringstream is(sql);
    std::string stm;
    is >> stm;
    std::transform(stm.begin(), stm.end(), stm.begin(), ::toupper);

    bool include = true;
    auto events = _filter->config().eventFilter;

    if (!events.empty())
    {
        include = std::find(events.begin(), events.end(), stm) != events.end();
    }
    if (include)
    {
        _stats->increment(stm);
        _filter->statisticsChanged(this); // ine lieu of threads
    }
    return mxs::FilterSession::routeQuery(buffer);
}

int64_t CounterSession::sessionId()
{
    return m_pSession->ses_id;
}
} // stm_counter
