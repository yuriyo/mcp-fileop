# MCP Server

This project implements a stateful MCP server for file operations using C++20, Drogon, Boost.Interprocess, Cpp-Taskflow, and Glaze.

## Features
- **Stateful Resources**: Preloaded files exposed as MCP resources
- Memory-mapped file I/O for efficient operations
- Two server implementations:
  - `mcp_stdio`: stdio-based MCP protocol (for VS Code integration)
  - `mcp_server`: HTTP-based API with SSE events
- Reference counting for file handlers
- Cross-platform file mapping
- Async task execution
- Structured error handling and security

## Build Instructions

1. Install dependencies using vcpkg:
   ```bash
   vcpkg install drogon boost-interprocess glaze taskflow openssl
   ```
2. Configure and build with CMake:
   ```bash
   cmake -B build
   cmake --build build
   ```
3. Run the stdio MCP server (for VS Code):
   ```bash
   ./build/mcp_stdio
   ```
   Or run the HTTP server:
   ```bash
   ./build/mcp_server
   ```

## VS Code Integration

To use this MCP server with VS Code and GitHub Copilot:

1. Build the stdio server: `cmake --build build`
2. Add to `~/Library/Application Support/Code/User/mcp.json`:
   ```json
   {
     "servers": {
       "mcp-fileop": {
         "type": "stdio",
         "command": "/absolute/path/to/build/mcp_stdio"
       }
     }
   }
   ```
3. Reload VS Code and use in Copilot Chat

See `.github/vscode-mcp-integration.md` for detailed integration guide.

## Project Structure
- `src/` - Source files and header files
- `tests/` - Unit and integration tests
- `CMakeLists.txt` - Build configuration

## API

- **Tools**: preload, read, read_multiple, close
- Note: For 'read' and 'read_multiple', results are returned as an array in `result.content[]` where each item contains a `parts[]` array with `{offset,size,text}`. `read` is equivalent to a single `content` / single `parts` entry.
- **Resources**: Lists preloaded files as resources
- **Protocol**: JSON-RPC 2.0 over stdio

### HTTP API (legacy)
- `POST /mcp` - Operations: preload, read, read_multiple, close
- `GET /events` - Server-Sent Events

See `mcp_server_design.md` for full design details.
