add_library(stmcounter SHARED stmcounter.cpp stmsession.cpp stmstats.cpp stopwatch.cpp)
target_link_libraries(stmcounter maxscale-common mysqlcommon)
set_target_properties(stmcounter PROPERTIES VERSION "1.0.0")
install_module(stmcounter core)
