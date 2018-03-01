#pragma once
/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#include "stopwatch.h"
#include <string>
#include <iosfwd>
#include <unordered_map> // <= really? Is it in all 4.4 versions
#include <chrono>

namespace stm_counter
{
// std::string does not have the small string optimization in gcc 4.4. For
// better speed, but still keeping it generic, I would roll my own or
// use facebook folly. A flyweight pattern could also be used (surprising
// speed increases for some data heavy usage patterns).
struct SessionID
{
    int sessionID;
    std::string user;
};

struct StmStats
{
    SessionID sid;
    std::string stmType;
    bool isSubQuery;
    int count;
};

std::ostream& operator<<(std::ostream& os, const StmStats& stats);

class StmStats
{
public:
    StmStats();
    void add(const SessionID& sid, const std::string& stmType, bool isSubQuery);
private:
    typedef std::unordered_map<std::string, StmStats> Cont;
};

std::ostream& operator<<(std::ostream& os, const StmStats& stats);

} // stm_counter
