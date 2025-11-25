# CMake test suite for MCP server
cmake_minimum_required(VERSION 3.16)
project(mcp_server_tests LANGUAGES CXX)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(test_segment_registry test_segment_registry.cpp ../src/SegmentRegistry.cpp ../src/MemorySegment.cpp)
target_link_libraries(test_segment_registry PRIVATE Boost::interprocess glaze::glaze)
