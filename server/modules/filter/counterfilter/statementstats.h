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
#include <cstring>
#include <iostream>

// Classes to keep statistics of events.
namespace stm_counter
{
// Session ID same as maxscale_sid
typedef uint64_t SessionId;

// Statement Id. A string so the event statistics can be kept for anything
// (so this is not really a statement counter).
typedef std::string StatementId;

// Statistics for a specific event. Not thread safe.
class StatementStats
{
    StatementStats(const StatementStats&);
    StatementStats& operator=(const StatementStats&);
public:
    explicit StatementStats(const StatementId& statementId, base::Duration timeWindow);
    StatementStats(StatementStats&&);  // can't be defaulted in gcc 4.4
    StatementStats& operator=(StatementStats&&); // can't be defaulted in gcc 4.4

    const StatementId& statementId() const {return _statementId;}
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
    StatementId _statementId;
    base::Duration _timeWindow;
    // One extra vector. Would need to templetize for one only. Keeping it simple for now.
    mutable std::vector<Timestamp> _timestampsOptimized;
    mutable std::vector<base::TimePoint> _timestampsExact;
    void _purge() const; // remove out of window stats
};

// Stats for a Session. Not thread safe.
class SessionStats
{
public:
    SessionStats(const SessionId& sessId, const std::string& user, base::Duration timeWindow);
    SessionStats(const SessionStats&) = delete;
    SessionStats& operator=(const SessionStats&) = delete;
    SessionStats(SessionStats &&);  // can't be defaulted in gcc 4.4
    SessionStats& operator=(SessionStats&&); // can't be defaulted in gcc 4.4

    const SessionId& sessionId() const {return _sessId;};
    base::Duration timeWindow() const {return _timeWindow;};
    void streamHumanReadable(std::ostream& os) const;
    void streamJson(std::ostream& os) const;
    const std::vector<StatementStats>& statementStats() const;
    bool empty() const; // no stats

    void increment(const StatementId& statementId);
    void purge(); // do a purge, for timing.
private:
    SessionId _sessId;
    std::string _user;
    base::Duration _timeWindow;
    mutable int _cleanupCountdown;
    mutable std::vector<StatementStats> _statementStats;

    void _purge() const; // remove out of window stats
};

// Custom made for the filter, but without dependencies back to the filter.
class CounterSession;
struct SessionData
{
    CounterSession* counterSession;
    SessionStats   sessionStats;

    // This section needed for gcc 4.4 to use move semantics and variadics.
    // Here be dragons! gcc-4.4 calls this constructor even with lvalues.
    SessionData(CounterSession*&& session_, SessionStats&& stats_);
    SessionData(const SessionData&) = delete;
    SessionData& operator=(const SessionData&) = delete;
    SessionData(SessionData&& sd);
    SessionData& operator=(SessionData&& sd);
};

void streamTotalsHumanReadable(std::ostream& os, const std::vector<SessionData>& sessions);
void streamTotalsJson(std::ostream& os, const std::vector<SessionData>& sdata);

std::ostream& operator<<(std::ostream& os, const StatementStats& stats);

} // stm_counter
