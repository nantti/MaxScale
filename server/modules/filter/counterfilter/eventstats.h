#pragma once
/*
    Copyright (c) Niclas Antti

    This software is released under the MIT License.
*/

#include "stopwatch.h"
#include <string>
#include <iosfwd>
#include <chrono>
#include <vector>
#include <memory>

// Keep statistics, or counts, of events over a
// given time window into the past (e.g. last 10 seconds).
namespace stm_counter
{
// Session ID same as maxscale sid
typedef uint64_t SessionId;

// Event Id. A string so the event statistics can be kept for anything.
typedef std::string EventId;

// Time series statistics for a specific event. Not thread safe.
class EventStat
{
    EventStat(const EventStat&);
    EventStat& operator=(const EventStat&);
public:
    explicit EventStat(const EventId& eventId, base::Duration timeWindow);
    EventStat(EventStat&&);  // can't be defaulted in gcc 4.4
    EventStat& operator=(EventStat&&); // can't be defaulted in gcc 4.4

    const EventId& eventId() const {return _eventId;}
    base::Duration timeWindow() const {return _timeWindow;}

    int count() const;
    void increment();

    // these defs need not be public when lambdas are available
    typedef std::chrono::time_point<base::Clock, std::chrono::seconds> Timepoint;
    struct Timestamp
    {
        Timepoint timepoint;
        int count;
        Timestamp(Timepoint p, int c) : timepoint(p), count(c) {}
    };
private:
    EventId _eventId;
    base::Duration _timeWindow;
    // One extra vector. Would need to templetize for one only. Keeping it simple for now.
    mutable std::vector<Timestamp> _timestampsOptimized;
    mutable std::vector<base::TimePoint> _timestampsExact;
    void _purge() const; // remove out of window stats
};

// Time series statistics for a Session (broadly a collection of EventStats). Not thread safe.
class SessionStats
{
public:
    SessionStats(const SessionId& sessId, const std::string& user, base::Duration timeWindow);
    SessionStats(const SessionStats&) = delete;
    SessionStats& operator=(const SessionStats&) = delete;
    SessionStats(SessionStats &&);  // can't be defaulted in gcc 4.4
    SessionStats& operator=(SessionStats&&); // can't be defaulted in gcc 4.4

    const SessionId& sessionId() const {return _sessId;};
    const std::string& user() const {return _user;};
    base::Duration timeWindow() const {return _timeWindow;};
    const std::vector<EventStat>& eventStats() const; // note, does a purge.
    void dump(std::ostream& os) const;
    bool empty() const; // no stats

    void increment(const EventId& eventId);
    void purge(); // do a purge, for timing.
private:
    SessionId _sessId;
    std::string _user;
    base::Duration _timeWindow;
    mutable int _cleanupCountdown;
    mutable std::vector<EventStat> _eventStats;

    void _purge() const; // remove out of window stats
};
// Custom made for the filter, but without dependencies back to the filter.
class CounterSession;
struct SessionData
{
    CounterSession* counterSession;
    std::unique_ptr<SessionStats>  sessionStats;

    // This section needed for gcc 4.4 to use move semantics and variadics.
    // Here be dragons! gcc-4.4 calls this constructor even with lvalues.
    SessionData(CounterSession*&& session_, std::unique_ptr<SessionStats> stats_);
    SessionData(const SessionData&) = delete;
    SessionData& operator=(const SessionData&) = delete;
    SessionData(SessionData&& sd);
    SessionData& operator=(SessionData&& sd);
};

void dump(std::ostream& os, const std::vector<SessionData>& sessions);
void dumpTotals(std::ostream& os, const std::vector<SessionData>& sessions);

std::ostream& operator<<(std::ostream& os, const EventStat& stats);

} // stm_counter
