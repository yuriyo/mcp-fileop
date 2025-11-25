MCP Server -- Full Implementation Design Document (English)

# 1. Goals

-   Provide a single MCP endpoint for operations: preload, read, close.
-   Maintain stateful in-memory mmap registry with reference counting.
-   Provide SSE event stream for notifications.
-   Support cross-platform mmap via Boost.Interprocess.
-   Use Drogon as HTTP server and Cpp-Taskflow as task executor.
-   Use Glaze for JSON parsing/serialization.

# 2. Technology Stack

-   vcpkg for dependency management.
-   Drogon for HTTP and SSE server capabilities.
-   Boost.Interprocess for file mapping across platforms.
-   Cpp-Taskflow for task scheduling and thread pooling.
-   Glaze for modern, fast JSON serialization.
-   CMake for build system.

# 3. Architecture

-   HTTP Layer handles /mcp and /events endpoints.
-   Controller parses requests and dispatches tasks.
-   Taskflow executor manages async and parallel tasks.
-   SegmentRegistry stores mmap segments, handlers, and refcounts.
-   MemorySegment wraps file_mapping and mapped_region.
-   SSE broadcaster pushes events to all connected clients.

# 4. Data Flow

-   Incoming request → JSON parse → registry/taskflow interaction →
    result → response.
-   SSE receives events: preload success, close, errors.

# 5. API

### POST /mcp:

Operations: - preload:
`{ "op": "preload", "params": { "path": "..." } }` - read:
`{ "op": "read", "params": { "handler": "...", "offset": N, "size": N, "format": "binary|hex|text" } }` -
close: `{ "op": "close", "params": { "handler": "..." } }`

### GET /events:

-   Returns Server-Sent Events.

# 6. Key Classes

## MemorySegment

-   Holds file mapping, mapped region, size, atomic refcount.

## SegmentRegistry

-   Maps canonical path → MemorySegment.
-   Maps handler → MemorySegment weak_ptr.
-   Provides thread-safe operations for preload/read/close.

## TaskflowManager

-   Provides Executor for async job execution.

## SSEBroadcaster

-   Maintains client queues and pushes event strings.

# 7. Concurrency

-   shared_mutex for registry.
-   atomic for refcounts.
-   read-only mmap region allows concurrent reads.
-   Taskflow executor manages parallel workload.

# 8. Error Handling

-   Structured JSON errors with code/message fields.
-   Path validation and canonicalization.
-   Bound checks for read offsets and size.

# 9. Security

-   Restrict file access to configured ROOT directory.
-   Enforce canonical paths.
-   Optional token-based authentication.
-   TLS support via Drogon.

# 10. Build & Packaging

-   `vcpkg install drogon boost-interprocess glaze taskflow openssl`
-   CMake links all dependencies.
-   Cross-platform builds supported.

# 11. Deployment

-   Recommend single-process for stateful registry.
-   Systemd or Docker for service management.
-   Optional reverse proxy with TLS termination.

# 12. Observability

-   Logging with operation, handler, path, and size.
-   Metrics counters for preload/read/close.
-   Unit tests for registry behavior.
-   Integration tests for API.

End of document.
