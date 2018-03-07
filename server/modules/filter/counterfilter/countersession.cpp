/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "counterfilter"

#include "countersession.h"
#include "counterfilter.h"
#include "eventstats.h"
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
}

CounterSession::~CounterSession()
{
}

int CounterSession::routeQuery(GWBUF *buffer)
{
    if (!modutil_is_SQL(buffer))
    {
        MXS_DEBUG("Not sql, ignore\n");
    }

    // I meant to report subqueries, but support would need to be added
    // to query_classifier (rather than parsing here). It would be enough
    // to get the expression tree and then use that here. It could be as
    // smart as one wants, e.g. "alter table", rather than just alter.

    std::string sql = to_sql(buffer);
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
        // The filter increments stats to keep locking in one place.
        _filter->increment(_stats, stm);
    }

    return mxs::FilterSession::routeQuery(buffer);
}

void CounterSession::close()
{
    // tell filter session is closing
    _filter->sessionClose(this);
}

int64_t CounterSession::sessionId()
{
    return m_pSession->ses_id;
}
} // stm_counter
