/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#include "stopwatch.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace base
{
StopWatch::StopWatch()
{
    restart();
}

Duration base::StopWatch::lap()
{
    return {Clock::now() - _start};
}

Duration StopWatch::restart()
{
    TimePoint now = Clock::now();
    Duration lap = now - _start;
    _start = now;
    return lap;
}
}

/********** OUTPUT ***********/
namespace
{
using namespace base;
struct TimeConvert
{
    double div;         // divide the value of the previous unit by this
    std::string suffix; // milliseconds, hours etc.
    double max_visual;  // threashold to switch to the next unit
};
// Will never get to centuries because the duration is a long carrying nanoseconds
TimeConvert convert[]
{
    {1, "ns", 1000}, {1000, "us", 1000},  {1000, "ms", 1000},
    {1000, "s", 60}, {60, "min", 60}, {60, "hours", 24},
    {24, "days", 365.25}, {365.25, "years", 10000},
    {100, "centuries", std::numeric_limits<double>::max()}
};

int convert_size = sizeof(convert) / sizeof(convert[0]);

}

namespace base
{
std::pair<double, std::string> dur_to_human_readable(Duration dur)
{
    using namespace std::chrono;
    double time = duration_cast<nanoseconds>(dur).count();
    bool negative = (time < 0) ? time = -time, true : false;

    for (int i = 0; i <= convert_size; ++i)
    {
        if (i == convert_size)
        {
            return std::make_pair(negative ? -time : time,
                                  convert[convert_size - 1].suffix);
        }

        time /= convert[i].div;

        if (time < convert[i].max_visual)
        {
            return std::make_pair(negative ? -time : time, convert[i].suffix);
        }
    }

    abort(); // should never get here
}

std::ostream& operator<<(std::ostream& os, Duration dur)
{
    auto p = dur_to_human_readable(dur);
    os << p.first << p.second;

    return os;
}
}

void test_stopwatch_output(std::ostream& os)
{
    long long dur[] =
    {
        400,    // 400ns
        5 * 1000, // 5us
        500 * 1000, // 500us
        1 * 1000000, // 1ms
        700 * 1000000LL, // 700ms
        5 * 1000000000LL, // 5s
        200 * 1000000000LL, // 200s
        5 * 60 * 1000000000LL, // 5m
        45 * 60 * 1000000000LL, // 45m
        130 * 60 * 1000000000LL, // 130m
        24 * 60 * 60 * 1000000000LL, // 24 hours
        3 * 24 * 60 * 60 * 1000000000LL, // 72 hours
        180 * 24 * 60 * 60 * 1000000000LL, // 180 days
        1000 * 24 * 60 * 60 * 1000000000LL // 1000 days
    };

    for (unsigned i = 0; i < sizeof(dur) / sizeof(dur[0]); ++i)
    {
        os << base::Duration(dur[i]) << std::endl;
    }
}
