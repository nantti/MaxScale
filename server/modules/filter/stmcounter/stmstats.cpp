#include "stmstats.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>

namespace stm_counter
{

const bool TimestampsOptimized = true;

// NOTE on this method. The statement-stats is simply a vector of timestamps where the
// size() of the vector is the count. The _purge() function purges out-of-window timestamps.
// The vector could grow large in very high volume. The "optimized" version uses a one second
// granularity, effectively grouping all events that happen in one second into a single entry.
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
            _timestampsOptimized.push_back(Timestamp(inSeconds, 1));
        }
        else
        {
            ++_timestampsOptimized.back().count;
        }

    }
    else
    {
        _timestampsExact.push_back(base::Clock::now());
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
    auto windowBegin = base::Clock::now() - _timeWindow;
    // code duplication alomst. Templetizing Timestamp would make this clearer.
    if (TimestampsOptimized)
    {
        if (!_timestampsOptimized.empty() &&
            _timestampsOptimized.front().timepoint < windowBegin)
        {
            auto ite = std::find_if(_timestampsOptimized.begin(), _timestampsOptimized.end(),
                                    TimePointLessEqual(windowBegin));
            _timestampsOptimized.erase(_timestampsOptimized.begin(), ite);
        }
    }
    else
    {
        if (!_timestampsExact.empty() && _timestampsExact.front() < windowBegin)
        {
            auto ite = std::find_if(_timestampsExact.begin(), _timestampsExact.end(),
                                    TimePointLessEqual(windowBegin));
            _timestampsExact.erase(_timestampsExact.begin(), ite);
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

SessionStats::SessionStats(const SessionId& sessId, base::Duration timeWindow) :
    _sessId(sessId), _timeWindow(timeWindow), _cleanupCountdown(CleanupCountdown)
{
}

void SessionStats::streamHumanReadable(std::ostream& os) const
{
    _purge();
    os << _sessId << "  window: " << _timeWindow << '\n';
    for (auto ite = _statementStats.begin(); ite != _statementStats.end(); ++ite)
    {
        os << *ite << '\n';
    }
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
        _statementStats.push_back(StatementStats(statementId, _timeWindow));
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

    // TODO configurable limit of StatementStats, and method to remove (the oldest ones).
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
    _cleanupCountdown = CleanupCountdown;
    // erase entries up to the first non-zero one
    auto ite = find_if(_statementStats.begin(), _statementStats.end(), NonZeroEntry());
    _statementStats.erase(_statementStats.begin(), ite);
}

std::ostream& operator<<(std::ostream& os, const StatementId& id)
{
    os << id.first << ' ' << (id.second ? "subquery:" : ":");
    return os;
}

std::ostream& operator<<(std::ostream& os, const SessionId& id)
{
    os << "Session: " << id.first << ' ' << id.second;
    return os;
}

std::ostream& operator<<(std::ostream& os, const StatementStats& stmStats)
{
    os << stmStats.statementId() << " " << stmStats.count();

    return os;
}


void streamTotalsHumanReadable(std::ostream& os, const std::vector<SessionStats>& sessions)
{
    std::map<StatementId, int> counts;
    for (auto session = sessions.begin(); session != sessions.end(); ++session)
    {
        const auto& stms = session->statementStats();
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
} // stm_counter
