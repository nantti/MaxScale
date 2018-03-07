add_library(counterfilter SHARED counterfilter.cpp countersession.cpp eventstats.cpp stopwatch.cpp eventout.cpp)
target_link_libraries(counterfilter maxscale-common mysqlcommon)
set_target_properties(counterfilter PROPERTIES VERSION "1.0.0")
install_module(counterfilter core)
