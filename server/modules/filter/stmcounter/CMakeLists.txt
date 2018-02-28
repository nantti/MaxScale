add_library(stmcounter SHARED stmcounter.cc stmsession.cc)
target_link_libraries(stmcounter maxscale-common mysqlcommon)
set_target_properties(stmcounter PROPERTIES VERSION "1.0.0")
install_module(stmcounter core)
