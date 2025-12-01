# CMake test suite for MCP server
cmake_minimum_required(VERSION 3.16)
project(mcp_server_tests LANGUAGES CXX)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(test_segment_registry test_segment_registry.cpp ../src/SegmentRegistry.cpp ../src/MemorySegment.cpp ../src/LineUtils.cpp)
target_link_libraries(test_segment_registry PRIVATE Boost::interprocess glaze::glaze)

add_executable(test_fileop_controller test_fileop_controller.cpp ../src/FileOpController.cpp ../src/SegmentRegistry.cpp ../src/MemorySegment.cpp ../src/LineUtils.cpp)
target_link_libraries(test_fileop_controller PRIVATE Boost::interprocess Drogon::Drogon)

add_executable(test_streaming_sse_progress test_streaming_sse_progress.cpp ../src/FileOpController.cpp ../src/SSEBroadcaster.cpp ../src/SegmentRegistry.cpp ../src/MemorySegment.cpp ../src/LineUtils.cpp)
target_link_libraries(test_streaming_sse_progress PRIVATE Boost::interprocess Drogon::Drogon)
add_executable(test_line_utils test_line_utils.cpp ../src/LineUtils.cpp)
target_link_libraries(test_line_utils PRIVATE)
add_executable(test_read_mixed_formats test_read_mixed_formats.cpp ../src/FileOpController.cpp ../src/SegmentRegistry.cpp ../src/MemorySegment.cpp ../src/LineUtils.cpp)
target_link_libraries(test_read_mixed_formats PRIVATE Boost::interprocess Drogon::Drogon)

add_executable(test_line_utils_fuzz test_line_utils_fuzz.cpp ../src/LineUtils.cpp)
target_link_libraries(test_line_utils_fuzz PRIVATE)
target_link_libraries(test_streaming_sse_progress PRIVATE Boost::interprocess Drogon::Drogon)
