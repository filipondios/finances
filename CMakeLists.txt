cmake_minimum_required(VERSION 3.23)
project(finances LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FINANCES_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Crow CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Depending on how it is installed, the SQLiteCpp target may or may not have the package prefix.
if(TARGET SQLiteCpp::SQLiteCpp)
  set(FINANCES_SQLITECPP SQLiteCpp::SQLiteCpp)
else()
  set(FINANCES_SQLITECPP SQLiteCpp)
endif()

# Warnings (the code base builds clean with all of these) and sanitizers.
add_library(finances_warnings INTERFACE)
if(MSVC)
  target_compile_options(finances_warnings INTERFACE /W4)
else()
  target_compile_options(finances_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wold-style-cast -Wdouble-promotion -Wformat=2 -Wnon-virtual-dtor
    $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>)
endif()

add_library(finances_sanitizers INTERFACE)
if(FINANCES_SANITIZE)
  if(MSVC)
    message(FATAL_ERROR "FINANCES_SANITIZE needs GCC or Clang")
  endif()
  target_compile_options(finances_sanitizers INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(finances_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

# Application logic without HTTP: domain, data access and use cases.
add_library(finances_core STATIC
  src/core/calendar.cpp
  src/core/entry_form.cpp
  src/core/money.cpp
  src/core/report.cpp
  src/db/entry_repository.cpp
  src/app/ledger.cpp)
target_include_directories(finances_core PUBLIC src)
target_compile_features(finances_core PUBLIC cxx_std_20)
target_link_libraries(finances_core
  PUBLIC ${FINANCES_SQLITECPP}
  PRIVATE finances_warnings finances_sanitizers)

add_executable(finances
  src/main.cpp
  src/web/routes.cpp
  src/web/views.cpp)
target_link_libraries(finances
  PRIVATE finances_core Crow::Crow Threads::Threads finances_warnings finances_sanitizers)

# Put templates/ and static/ next to the binary, so it also runs from the
# build folder: `FINANCES_HOME=<build dir> ./finances`.
add_custom_command(TARGET finances POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/templates $<TARGET_FILE_DIR:finances>/templates
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/static $<TARGET_FILE_DIR:finances>/static
  COMMENT "Copying templates/ and static/")

enable_testing()

# Unit tests (Catch2 v3). Skip with -DFINANCES_BUILD_TESTS=OFF.
option(FINANCES_BUILD_TESTS "Build the unit tests (needs Catch2 3)" ON)
if(FINANCES_BUILD_TESTS)
  find_package(Catch2 3 CONFIG REQUIRED)
  add_executable(finances_tests
    tests/test_calendar.cpp
    tests/test_entry_form.cpp
    tests/test_ledger.cpp
    tests/test_migrations.cpp
    tests/test_money.cpp
    tests/test_report.cpp
    tests/test_text.cpp)
  target_link_libraries(finances_tests
    PRIVATE finances_core Catch2::Catch2WithMain finances_warnings finances_sanitizers)
  include(Catch)
  catch_discover_tests(finances_tests)
endif()

# End-to-end smoke test against the real server.
add_test(NAME smoke COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh $<TARGET_FILE:finances>)
