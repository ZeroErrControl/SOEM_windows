
set(SOURCES simple_test.c)
add_executable(simple_test ${SOURCES})
target_link_libraries(simple_test soem)
install(TARGETS simple_test DESTINATION bin)

#set(SOURCES simple_test.c)
add_executable(eRob_test eRob_test.c)
target_link_libraries(eRob_test soem)
install(TARGETS eRob_test DESTINATION bin)

#set(SOURCES simple_test.c)
add_executable(dc_test dc_test.c)
target_link_libraries(dc_test soem)
install(TARGETS dc_test DESTINATION bin)