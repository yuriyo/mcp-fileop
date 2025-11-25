cmake_minimum_required(VERSION 3.16)
project(mcp_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies via vcpkg
find_package(Drogon REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem interprocess)

# Check if optional dependencies are available
find_package(taskflow QUIET)
find_package(glaze QUIET)

# HTTP-based server (original)
if(taskflow_FOUND AND glaze_FOUND)
    add_executable(mcp_server
        src/main.cpp
        src/SegmentRegistry.cpp
        src/MemorySegment.cpp
        src/TaskflowManager.cpp
        src/SSEBroadcaster.cpp
    )
    # Link libraries
    target_link_libraries(mcp_server PRIVATE Drogon::Drogon Boost::interprocess Taskflow::Taskflow glaze::glaze)
endif()

# stdio-based MCP server for VS Code
add_executable(mcp_stdio
    src/mcp_stdio.cpp
    src/SegmentRegistry.cpp
    src/MemorySegment.cpp
)

# Link libraries for stdio version
target_link_libraries(mcp_stdio PRIVATE Drogon::Drogon Boost::interprocess)

# Streaming MCP server (HTTP + SSE + WebSocket)
add_executable(mcp_stream
    src/mcp_stream.cpp
    src/SegmentRegistry.cpp
    src/MemorySegment.cpp
    src/SSEBroadcaster.cpp
)

# Link libraries for streaming version
target_link_libraries(mcp_stream PRIVATE Drogon::Drogon Boost::interprocess)

# Enable testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
