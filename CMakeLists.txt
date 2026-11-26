cmake_minimum_required(VERSION 3.16)
project(finances LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Crow CONFIG REQUIRED)
find_package(SQLite3 CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(finances src/main.cpp)
target_link_libraries(finances PRIVATE Crow::Crow SQLite::SQLite3 Threads::Threads)

enable_testing()
add_test(NAME smoke COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh $<TARGET_FILE:finances>)
