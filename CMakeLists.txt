cmake_minimum_required(VERSION 3.16)
project(finances LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Crow CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Depending on how it is installed, the SQLiteCpp target may or may not have the package prefix.
if(TARGET SQLiteCpp::SQLiteCpp)
  set(FINANCES_SQLITECPP SQLiteCpp::SQLiteCpp)
else()
  set(FINANCES_SQLITECPP SQLiteCpp)
endif()

add_library(finances_warnings INTERFACE)
if(MSVC)
  target_compile_options(finances_warnings INTERFACE /W4)
else()
  target_compile_options(finances_warnings INTERFACE -Wall -Wextra -Wpedantic)
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
  PRIVATE finances_warnings)

add_executable(finances
  src/main.cpp
  src/web/routes.cpp
  src/web/views.cpp)
target_link_libraries(finances
  PRIVATE finances_core Crow::Crow Threads::Threads finances_warnings)

enable_testing()
add_test(NAME smoke COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh $<TARGET_FILE:finances>)
