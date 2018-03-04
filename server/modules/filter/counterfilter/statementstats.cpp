#include "statementstats.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
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
// The gcc 4.4 does not use the small string optimization, so I tested with a little
// QuckString class instead 30% faster.
const bool TimestampsOptimized = false;

// NOTE on this method. The statement-stats is simply a vector of timestamps where the
// size() of the vector is the count. The _purge() function purges out-of-window timestamps.
// The vector grows large in very volume and long time windows. The "optimized" version uses
// a one second granularity, effectively grouping all events that happen in one second
// into a single entry.
StatementStats::StatementStats(const StatementId& statementId, base::Duration timeWindow) :
    _statementId(statementId), _timeWindow(timeWindow)
{
    increment();
}

void StatementStats::increment()
{
    // FIXME make configurable. Should purge once in awhile.
    if (TimestampsOptimized)
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
    bool operator()(const StatementStats::Timestamp& rhs) const
    {
        return lhs <= rhs.timepoint;
    }
    bool operator()(base::TimePoint rhs) const
    {
        return lhs <= rhs;
    }
};
}

void StatementStats::_purge() const
{
    base::StopWatch sw;
    auto windowBegin = base::Clock::now() - _timeWindow;

    // code duplication, alomst. Templetizing Timestamp would not make this clearer.
    if (TimestampsOptimized)
    {
        if (!_timestampsOptimized.empty() &&
            _timestampsOptimized.front().timepoint < windowBegin)
        {
            auto ite = std::find_if(_timestampsOptimized.begin(), _timestampsOptimized.end(),
                                    TimePointLessEqual(windowBegin));
            _timestampsOptimized.erase(_timestampsOptimized.begin(), ite);
            //std::cout << "StatementStats::_purge " << sw.lap() << '\n';
        }
    }
    else
    {
        if (!_timestampsExact.empty() && _timestampsExact.front() < windowBegin)
        {
            auto ite = std::find_if(_timestampsExact.begin(), _timestampsExact.end(),
                                    TimePointLessEqual(windowBegin));
            _timestampsExact.erase(_timestampsExact.begin(), ite);
            //std::cout << "StatementStats::_purge " << sw.lap() << '\n';
        }
    }
}

int StatementStats::count() const
{
    _purge();
    if (TimestampsOptimized)
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

const int CleanupCountdown = 12;

SessionStats::SessionStats(const SessionId& sessId, const std::string &user, base::Duration timeWindow) :
    _sessId(sessId), _user(user), _timeWindow(timeWindow), _cleanupCountdown(CleanupCountdown)
{
}

void SessionStats::streamJson(std::ostream& os) const
{
    _purge();
}

const std::vector<StatementStats> &SessionStats::statementStats() const
{
    _purge();
    return _statementStats;
}

namespace
{
struct MatchStmId
{
    StatementId statementId;
    MatchStmId(const StatementId& id) : statementId(id) {};
    bool operator()(const StatementStats& stats) const
    {
        return statementId == stats.statementId();
    }
};
}

void SessionStats::increment(const StatementId& statementId)
{
    // What I really need here is an LRU-kind of structure, but a full blown LRU
    // would mosty likely slow things down. Needs to be measured. Using vectors of
    // not massive amounts of data, moving memory is very fast (if data is a pod).
    // So the algo would be:
    // 1. If an entry is new, push_back.
    // 2. If the entry exists, move all entries after it one step to the left, put the entry last.
    // 3. Always search in reverse order since entries towards the end are more likely to be used.
    // 4. Once in awhile purge entries with count()==0.
    // Need to measure.

    // Look in reverse order, the entry is likely to be towards the end
    auto ite = find_if(_statementStats.begin(), _statementStats.end(), MatchStmId(statementId));
    if (ite == _statementStats.end())
    {
        _statementStats.emplace_back(statementId, _timeWindow);
    }
    else
    {
        // This would be very fast with c++11, since pod data would simply be memmove:ed.
        ite->increment();
        auto middle = ite;
        ++middle;
        std::rotate(ite, middle, _statementStats.end());
    }

    // TODO make this configurable, if needed.
    if (!--_cleanupCountdown)
    {
        _purge();
    }
}

void SessionStats::purge()
{
    _purge();
}
namespace
{
struct NonZeroEntry
{
    bool operator()(const StatementStats& stats)
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
    auto ite = find_if(_statementStats.begin(), _statementStats.end(), NonZeroEntry());
    // The gcc 4.4 vector::erase bug only happens if iterators are the same.
    if (ite != _statementStats.begin())
    {
        _statementStats.erase(_statementStats.begin(), ite);
        //std::cout << "SessionStats::_purge " << sw.lap() << '\n';
    }
}

// OUTPUT
void SessionStats::streamHumanReadable(std::ostream& os) const
{
    _purge();
    os << "Session: " << _sessId << ' ' << _user << ". Window: " << _timeWindow << '\n';
    for (auto ite = _statementStats.begin(); ite != _statementStats.end(); ++ite)
    {
        os << *ite << '\n';
    }
}

void streamTotalsHumanReadable(std::ostream& os, const std::vector<SessionData>& sessions)
{
    std::map<StatementId, int> counts;
    for (auto session = sessions.begin(); session != sessions.end(); ++session)
    {
        const auto& stms = session->sessionStats.statementStats();
        for (auto stm = stms.begin(); stm != stms.end(); ++stm)
        {
            counts[stm->statementId()] += stm->count();
        }
    } // stm_counter

    os << "Totals:\n";
    for (auto ite = counts.begin(); ite != counts.end(); ++ite)
    {
        os << "  " << ite->first << " " << ite->second << '\n';
    }
};

std::ostream& operator<<(std::ostream& os, const StatementId& id)
{
    os << id.first << ' ' << (id.second ? "subquery:" : ":");
    return os;
}

std::ostream& operator<<(std::ostream& os, const StatementStats& stmStats)
{
    os << stmStats.statementId() << " " << stmStats.count();

    return os;
}

StatementStats &StatementStats::operator=(StatementStats && ss)
{
    _statementId = std::move(ss._statementId);
    _timeWindow = std::move(ss._timeWindow);
    _timestampsOptimized = std::move(ss._timestampsOptimized);
    _timestampsExact = std::move(ss._timestampsExact);

    return *this;
}

// EXTRA
// This section needed for gcc 4.4 to use move semantics and variadics.
StatementStats::StatementStats(StatementStats && ss) :
    _statementId(std::move(ss._statementId)),
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
    _statementStats(std::move(ss._statementStats))
{
}

SessionStats &SessionStats::operator=(SessionStats&& ss)
{
    _sessId = std::move(ss._sessId);
    _user = std::move(ss._user);
    _timeWindow = std::move(ss._timeWindow);
    _cleanupCountdown = std::move(ss._cleanupCountdown);
    _statementStats = std::move(ss._statementStats);

    return *this;
}

SessionData::SessionData(CounterSession*&& session_, SessionStats&& sessionStats_) :
    counterSession(std::move(session_)), sessionStats(std::move(sessionStats_))
{}
SessionData::SessionData(SessionData&& sd) :
    counterSession(std::move(sd.counterSession)), sessionStats(std::move(sd.sessionStats))
{}
SessionData& SessionData::operator=(SessionData&& sd)
{
    counterSession = std::move(sd.counterSession);
    sessionStats = std::move(sd.sessionStats);
    return *this;
}
} // stm_counter
