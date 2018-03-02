#pragma once
/*
    Copyright (c) Niclas Antti

    This software is released under the MIT License.
*/

// to proper place:
// Moving time-window statistics

#include "stopwatch.h"
#include <string>
#include <iosfwd>
#include <chrono>
#include <vector>

// Filter that keeps statistics of sql statements. Using a moving window-of-time
// so that the stats, when requested, are as of "now". The purpose is to report
// human readable text or json.
namespace stm_counter
{

// This compiles with g++ 4.4.7

// std::string does not have the small string optimization in gcc 4.4. For
// better speed, but still keeping it generic, I would roll my own or
// use facebook folly. A flyweight pattern could also be used (surprising
// speed increases for some data heavy usage patterns).

// ascii art here to show the classes

// Here I use underscore-lovercase for protected and private members
// (it is standards compliant). OF course, in the end I will always follow
// the coding standards, or the standard in surrounding code when modifying
// something.

// Session ID. {int maxscale_sid; std::string user}. User included for better output.
typedef std::pair<uint64_t, std::string> SessionId;

// Statement Id, {std::string statement, bool isSubQuery}.
typedef std::pair<std::string, bool> StatementId;

// Statistics of a SQL statement. Not thread safe.
class StatementStats
{
public:
    StatementStats(const StatementId& statementId, base::Duration timeWindow);
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
    mutable std::vector<Timestamp> _timestampsOptimized;
    mutable std::vector<base::TimePoint> _timestampsExact;  // not optimized
    void _purge() const; // remove out of window stats
};

// Stats for a Session. Not thread safe.
class SessionStats
{
public:
    SessionStats(const SessionId& sessId, base::Duration timeWindow);
    const SessionId& sessionId() const {return _sessId;};
    base::Duration timeWindow() const {return _timeWindow;};
    void streamHumanReadable(std::ostream& os) const;
    void streamJson(std::ostream& os) const;
    const std::vector<StatementStats>& statementStats() const;

    void increment(const StatementId& statementId);
private:
    SessionId _sessId;
    base::Duration _timeWindow;
    mutable int _cleanupCountdown;
    mutable std::vector<StatementStats> _statementStats;

    void _purge() const; // remove out of window stats
};

void streamTotalsHumanReadable(std::ostream& os, const std::vector<SessionStats>& sessions);
void streamTotalsJson(std::ostream& os, const std::vector<SessionStats>& sessions);

std::ostream& operator<<(std::ostream& os, const SessionId& id);
std::ostream& operator<<(std::ostream& os, const StatementId& id);
std::ostream& operator<<(std::ostream& os, const StatementStats& stats);

} // stm_counter
