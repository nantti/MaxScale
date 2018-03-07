#pragma once
/*
    Copyright (c) Niclas Antti

    This software is released under the MIT License.
*/

#include "eventstats.h"
#include <iosfwd>

// Event statistics utput to other than human readable file types. Keeps
// the events stats only depending on c++ std.

namespace stm_counter
{
void dumpJson(std::ostream& os, const std::vector<SessionData>& sessions);
void dumpJsonTotals(std::ostream& os, const std::vector<SessionData>& sessions);

} // stm_counter
