#include "eventstats.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>

namespace stm_counter
{
// Measured inserting a mixed set of 10M entires to 10 sessions
// on an AMD FX(tm)-8350 in a single thread. Measured from top level,
// so includes a little more than just inserting and purging.
// Optimized  Non-optimized  Operation
// 1.06s       1.14s         Insert 10M in a thight loop. Not much difference.
// 15us        22ms          Single purge. Optimized > 1400x faster than non-optimized.
// Memory usage difference is of course huge. For this example, the optimized version
// uses numSessions*numStatementTypes*numSeconds = 10*4*2 = 80 entries, while the
// non-optimized version uses 10,000,000 entries.
// gcc 4.4 does not have the small string optimization, so I tested with a little
// QuckString class instead (in git), which made numbers above about 35% faster.
namespace
{
bool timestampsOptimized = true; // outside of class to save on storage
}

// NOTE on this method. The event-stats is simply a vector of timestamps where the
// size() of the vector is the count. The _purge() function purges out-of-window timestamps.
// The vector grows large with high volume or long time windows. The "optimized" version uses
// a one second granularity, effectively grouping all events that happen in one second
// into a single entry. The choise of optimized vs exact is made based on window size.
EventStat::EventStat(const EventId& eventId, base::Duration timeWindow) :
    _eventId(eventId), _timeWindow(timeWindow)
{
    // Maybe this should be configurable, or even dynamic based on volume. Should
    // not be less than 10 seconds.
    timestampsOptimized = (timeWindow < std::chrono::seconds(30)) ? false : true;
    increment();
}

void EventStat::increment()
{
    if (timestampsOptimized)
    {
        using namespace std::chrono;
        Timepoint inSeconds = time_point_cast<seconds>(base::Clock::now());
        if (_timestampsOptimized.empty() || _timestampsOptimized.back().timepoint != inSeconds)
        {
            _timestampsOptimized.emplace_back(inSeconds, 1);
        }
        else
        {
            ++_timestampsOptimized.back().count;
        }

    }
    else
    {
        _timestampsExact.emplace_back(base::Clock::now());
    }
}

namespace
{
struct TimePointLessEqual
{
    base::TimePoint lhs;
    TimePointLessEqual(base::TimePoint tp) : lhs(tp) {}
    bool operator()(const EventStat::Timestamp& rhs) const
    {
        return lhs <= rhs.timepoint;
    }
    bool operator()(base::TimePoint rhs) const
    {
        return lhs <= rhs;
    }
};
}

void EventStat::_purge() const
{
    base::StopWatch sw;
    auto windowBegin = base::Clock::now() - _timeWindow;

    // code duplication, almost. Templetizing would not make this much clearer,
    // but should be made for a production utility, that also needs to make thread safety
    // a template policy.
    if (timestampsOptimized)
    {
        if (!_timestampsOptimized.empty() &&
            _timestampsOptimized.front().timepoint < windowBegin)
        {
            // never has a lot of entries, unless the time window is extremly long (days).
            auto ite = std::find_if(_timestampsOptimized.begin(), _timestampsOptimized.end(),
                                    TimePointLessEqual(windowBegin));
            _timestampsOptimized.erase(_timestampsOptimized.begin(), ite);
            //std::cout << "Optimized EventStat::_purge " << sw.lap() << '\n';
        }
    }
    else
    {
        if (!_timestampsExact.empty() && _timestampsExact.front() < windowBegin)
        {
            // should be very fast, walking a vector of integers.
            auto ite = std::find_if(_timestampsExact.begin(), _timestampsExact.end(),
                                    TimePointLessEqual(windowBegin));
            _timestampsExact.erase(_timestampsExact.begin(), ite);
            //std::cout << "Exact EventStat::_purge " << sw.lap() << '\n';
        }
    }
}

int EventStat::count() const
{
    _purge();
    if (timestampsOptimized)
    {
        int count {0};
        //#pragma omp parallel for reduction(max : count)
        for (auto ite = _timestampsOptimized.begin(); ite != _timestampsOptimized.end(); ++ite)
        {
            count += ite->count;
        }
        return count;
    }
    else
    {
        return _timestampsExact.size();
    }
}

// Force a purge once in awhile, could be configurable.
const int CleanupCountdown = 100000;

SessionStats::SessionStats(const SessionId& sessId,
                           const std::string &user, base::Duration timeWindow) :
    _sessId(sessId), _user(user), _timeWindow(timeWindow),
    _cleanupCountdown(CleanupCountdown)
{
}

const std::vector<EventStat> &SessionStats::eventStats() const
{
    _purge();
    return _eventStats;
}

bool SessionStats::empty() const
{
    _purge();
    return _eventStats.empty();
}

namespace
{
struct MatchEventId
{
    EventId eventId;
    MatchEventId(const EventId& id) : eventId(id) {};
    bool operator()(const EventStat& stats) const
    {
        return eventId == stats.eventId();
    }
};
}

void SessionStats::increment(const EventId& eventId)
{
    // Always put the incremented entry (latest timestamp) last in the vector (using
    // rotate). This means the vector is ordered so that expired entries are always first.

    // Find in reverse, the entry is more likely to be towards the end. Actually no,
    // for some reason the normal search is slightly faster when measured.
    auto ite = find_if(_eventStats.begin(), _eventStats.end(),
                       MatchEventId(eventId));
    if (ite == _eventStats.end())
    {
        _eventStats.emplace_back(eventId, _timeWindow);
    }
    else
    {
        ite->increment();
        // rotate so that the entry becomes the last one
        auto next = std::next(ite);
        std::rotate(ite, next, _eventStats.end());
    }

    if (!--_cleanupCountdown)
    {
        _purge();
    }
}

void SessionStats::purge() // this is just for testing
{
    _purge();
}

namespace
{
struct NonZeroEntry
{
    bool operator()(const EventStat& stats)
    {
        return stats.count() != 0;
    }
};
}

void SessionStats::_purge() const
{
    base::StopWatch sw;
    _cleanupCountdown = CleanupCountdown;
    // erase entries up to the first non-zero one
    auto ite = find_if(_eventStats.begin(), _eventStats.end(), NonZeroEntry());
    // The gcc 4.4 vector::erase bug only happens if iterators are the same.
    if (ite != _eventStats.begin())
    {
        _eventStats.erase(_eventStats.begin(), ite);
        //std::cout << "SessionStats::_purge " << sw.lap() << '\n';
    }
}

// OUTPUT follows

std::ostream& operator<<(std::ostream& os, const EventStat& eventStats)
{
    os << eventStats.eventId() << ": " << eventStats.count();

    return os;
}

void dumpHeader(std::ostream& os, const SessionStats* stats, const std::string& type)
{
    base::TimePoint tp = base::Clock::now();
    os << type << ": Time:" << tp
       << " Time Window: " << stats->timeWindow() << '\n';
}

void SessionStats::dump(std::ostream& os) const
{
    _purge();
    if (!_eventStats.empty())
    {
        os << "  Session: " << _sessId << " User: " << _user << '\n';
        for (auto ite = _eventStats.begin(); ite != _eventStats.end(); ++ite)
        {
            os << "    " << *ite << '\n';
        }
    }
}

void dump(std::ostream& os, const std::vector<SessionData>& sessions)
{
    if (sessions.empty()) { return; }

    dumpHeader(os, sessions[0].sessionStats.get(), "SQL Statistics");
    for (auto session = sessions.begin(); session != sessions.end(); ++session)
    {
        session->sessionStats->dump(os);
    }
}

void dumpTotals(std::ostream& os, const std::vector<SessionData> &sessions)
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
    } // stm_counter

    if (!counts.empty())
    {
        dumpHeader(os, sessions[0].sessionStats.get(), "SQL Statistics Totals");
        for (auto ite = counts.begin(); ite != counts.end(); ++ite)
        {
            os << "  " << ite->first << ": " << ite->second << '\n';
        }
    }
}

// EXTRA
// This section needed for gcc 4.4, to use move semantics and variadics.

EventStat &EventStat::operator=(EventStat && ss)
{
    _eventId = std::move(ss._eventId);
    _timeWindow = std::move(ss._timeWindow);
    _timestampsOptimized = std::move(ss._timestampsOptimized);
    _timestampsExact = std::move(ss._timestampsExact);

    return *this;
}

EventStat::EventStat(EventStat && ss) :
    _eventId(std::move(ss._eventId)),
    _timeWindow(std::move(ss._timeWindow)),
    _timestampsOptimized(std::move(ss._timestampsOptimized)),
    _timestampsExact(std::move(ss._timestampsExact))
{
}

SessionStats::SessionStats(SessionStats&& ss) :
    _sessId(std::move(ss._sessId)),
    _user(std::move(ss._user)),
    _timeWindow(std::move(ss._timeWindow)),
    _cleanupCountdown(std::move(ss._cleanupCountdown)),
    _eventStats(std::move(ss._eventStats))
{
}

SessionStats &SessionStats::operator=(SessionStats&& ss)
{
    _sessId = std::move(ss._sessId);
    _user = std::move(ss._user);
    _timeWindow = std::move(ss._timeWindow);
    _cleanupCountdown = std::move(ss._cleanupCountdown);
    _eventStats = std::move(ss._eventStats);

    return *this;
}

SessionData::SessionData(CounterSession*&& session_, std::unique_ptr<SessionStats> sessionStats_) :
    counterSession(std::move(session_)), sessionStats(std::move(sessionStats_))
{
}
SessionData::SessionData(SessionData&& sd) :
    counterSession(std::move(sd.counterSession)), sessionStats(std::move(sd.sessionStats))
{
    sd.counterSession = 0;
}
SessionData& SessionData::operator=(SessionData&& sd)
{
    counterSession = std::move(sd.counterSession);
    sessionStats = std::move(sd.sessionStats);
    sd.counterSession = 0;
    return *this;
}

} // stm_counter
