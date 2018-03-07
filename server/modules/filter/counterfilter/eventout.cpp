#include "eventout.h"
#include <maxscale/jansson.hh>
#include <iostream>
#include <sstream>
#include <map>

namespace stm_counter
{
namespace
{
// Create a report object in top, return report object (which is top for now).
json_t* makeReport(json_t* top, const std::string& reportType, base::Duration tw)
{
    base::TimePoint tp = base::Clock::now();
    std::ostringstream ss;
    ss << tp;
    json_object_set_new(top, "Report Type", json_string(reportType.c_str()));
    json_object_set_new(top, "Report Time", json_string(ss.str().c_str()));
    ss.str("");
    ss << tw;
    json_object_set_new(top, "Time Window", json_string(ss.str().c_str()));
    return top;
}
}

void dumpJson(std::ostream& os, const std::vector<SessionData>& sessions)
{
    if (!sessions.empty())
    {
        json_t* top = json_object();
        json_t* report = makeReport(top, "SQL Statistics",
                                    sessions[0].sessionStats->timeWindow());
        json_t* sessionArr = json_array();
        json_object_set_new(report, "Sessions", sessionArr);
        for (auto session = sessions.begin(); session != sessions.end(); ++session)
        {
            const auto& stats = *session->sessionStats;
            json_t* sessEntry = json_object();
            json_array_append_new(sessionArr, sessEntry);
            json_object_set_new(sessEntry, "Session ID", json_integer(stats.sessionId()));
            json_object_set_new(sessEntry, "User", json_string(stats.user().c_str()));
            json_t* eventarr = json_array();
            json_object_set_new(sessEntry, "Events", eventarr);
            const auto& events = stats.eventStats();
            for (auto ite = events.begin(); ite != events.end(); ++ite)
            {
                json_t* entry = json_object();
                json_array_append_new(eventarr, entry);
                json_object_set_new(entry, "name", json_string(ite->eventId().c_str()));
                json_object_set_new(entry, "count", json_integer(ite->count()));
            }
        }

        os << maxscale::json_dump(top) << '\n';
    }
}

void dumpJsonTotals(std::ostream& os, const std::vector<SessionData>& sessions)
{
    if (sessions.empty()) { return; }

    std::map<EventId, int> counts;
    for (auto session = sessions.begin(); session != sessions.end(); ++session)
    {
        const auto& events = session->sessionStats->eventStats();
        for (auto event = events.begin(); event != events.end(); ++event)
        {
            counts[event->eventId()] += event->count();
        }
    }

    if (!counts.empty())
    {
        json_t* top = json_object();
        json_t* report = makeReport(top, "SQL Statistics Totals",
                                    sessions[0].sessionStats->timeWindow());
        json_t* eventarr = json_array();
        json_object_set_new(report, "Events", eventarr);
        for (auto ite = counts.begin(); ite != counts.end(); ++ite)
        {
            json_t* entry = json_object();
            json_object_set_new(entry, "name", json_string(ite->first.c_str()));
            json_object_set_new(entry, "count", json_integer(ite->second));
            json_array_append_new(eventarr, entry);
        }
        os << maxscale::json_dump(top) << '\n';
    }
}

} // stm_counter
