# ### ## # ## ### ## # ## ### ## # ## ### ## # ## ### ## # ## ### ## # ## ###
#
# Unit tests. The integration suite lives next door in tests/run_tests.py and
# drives the binary; these link the library directly.
#
# ### ## # ## ### ## # ## ### ## # ## ### ## # ## ### ## # ## ### ## # ## ###
project(Tester)
set(BINARY_NAME tester)

set(TEST_SOURCE_FILES
  testerMain.cpp
  testDocument.cpp
  testGlobMatch.cpp
  testIgnoreRules.cpp
  testParser.cpp
  testReport.cpp
  testLanguages.cpp
)

add_executable(${BINARY_NAME} ${TEST_SOURCE_FILES})

# By target, not by path to the .a, so build ordering is correct
target_link_libraries(${BINARY_NAME} PRIVATE gtest MoosecountLib)
target_compile_options(${BINARY_NAME} PRIVATE ${MOOSECOUNT_WARNINGS})

add_test(NAME unit COMMAND ${BINARY_NAME})
