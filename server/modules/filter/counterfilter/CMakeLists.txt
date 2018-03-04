add_library(counterfilter SHARED counterfilter.cpp countersession.cpp statementstats.cpp stopwatch.cpp)
target_link_libraries(counterfilter maxscale-common mysqlcommon)
set_target_properties(counterfilter PROPERTIES VERSION "1.0.0")
install_module(counterfilter core)
