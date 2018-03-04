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
#include <cstring>

#include <iostream> // FIXME, remove

#include <iostream>
#include <vector>
#include <cstring>

namespace base
{
template <int N>
struct QuickString
{
    QuickString(const char* ch)
    {
        std::strncpy(_buf, ch, N);
        _buf[N - 1] = 0;
    }
    QuickString(const QuickString& other)
    {
        std::memcpy(_buf, other._buf, N);
    }

    operator const char*() const { return _buf;}
    const char* c_str() const { return _buf;}

    char _buf[N];
};
template<int N>
bool operator==(const QuickString<N>& lhs, const QuickString<N>& rhs)
{
    return std::strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

template<int N>
bool operator<(const QuickString<N>& lhs, const QuickString<N>& rhs)
{
    return strcmp(lhs.c_str(), rhs.c_str()) < 0;
}

template <int N>
std::ostream& operator<<(std::ostream& os, const QuickString<N>& ss)
{
    os << ss._buf;
    return os;
}
} // base

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

// Session ID same as maxscale_sid
typedef uint64_t SessionId;


typedef base::QuickString<32> StatementString;
//typedef std::string StatementString;

// Statement Id, {string statement, bool isSubQuery}.
typedef std::pair<StatementString, bool> StatementId;

// Statistics of a SQL statement. Not thread safe. Copies made, needs C++11.
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
    // One extra vector. Would need to templetize for one only.
    mutable std::vector<Timestamp> _timestampsOptimized;
    mutable std::vector<base::TimePoint> _timestampsExact;  // not optimized
    void _purge() const; // remove out of window stats
};

// Stats for a Session. Not thread safe. Copiable, needs C++11.
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

    void increment(const StatementId& statementId);
    void purge(); // do a normal purge, for testing.
private:
    SessionId _sessId;
    std::string _user;
    base::Duration _timeWindow;
    mutable int _cleanupCountdown;
    mutable std::vector<StatementStats> _statementStats;

    void _purge() const; // remove out of window stats
};

// For the filter
class CounterSession;
struct SessionData
{
    CounterSession* counterSession;
    SessionStats    sessionStats;

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

std::ostream& operator<<(std::ostream& os, const StatementId& id);
std::ostream& operator<<(std::ostream& os, const StatementStats& stats);

} // stm_counter
