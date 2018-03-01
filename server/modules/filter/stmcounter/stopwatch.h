/*
* Copyright (c) Niclas Antti
*
* This software is released under the MIT License.
*/

#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <chrono>

#include <iosfwd>
#include <utility>

namespace base
{
typedef std::chrono::system_clock Clock;

struct Duration : public Clock::duration // for ADL
{
    Duration() = default;
    Duration(long long l) : Clock::duration(l) {}
    Duration(Clock::duration d) : Clock::duration(d) {}
};

class StopWatch
{
public:
    // Starts the stopwatch. It is always running.
    StopWatch();
    // Get elapsed time.
    Duration lap();
    // Get elapsed time, reset StopWatch.
    Duration restart();
private:
    typedef std::chrono::time_point<Clock, Duration> TimePoint;
    TimePoint _start;
};

// Returns the value as a double and string adjusted to a suffix, like ms for milliseconds.
std::pair<double, std::string> dur_to_human_readable(Duration dur);

// Human readable output. No standard library for it yet.
std::ostream& operator<<(std::ostream&, Duration dur);
} // base

#endif // STOPWATCH_H
