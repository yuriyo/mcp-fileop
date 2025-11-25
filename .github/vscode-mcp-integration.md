# VS Code MCP Integration Guide

## Overview
This guide explains how to integrate the mcp-fileop server with VS Code's GitHub Copilot MCP feature. The server maintains stateful resources for preloaded files, allowing efficient memory-mapped file operations.

## Key Features
- **Stateful Resources**: Preloaded files are exposed as MCP resources that persist across operations
- **Memory-Mapped I/O**: Efficient file access using OS-level memory mapping
- **Tools**: preload, read, and close operations for managing file handlers
- **Resources API**: List and read preloaded files via the MCP resources interface

## Prerequisites
1. VS Code with GitHub Copilot extension installed
2. Built mcp_stdio binary (`./build/mcp_stdio`) - uses stdio-based MCP protocol
3. Built mcp_server binary (`./build/mcp_server`) - HTTP-based server (for direct API access)

## Integration Steps

### 1. Configure VS Code User Settings

Add the MCP server to your VS Code user settings (`mcp.json`):

**Location**: `~/Library/Application Support/Code/User/mcp.json` (macOS)

```json
{
  "servers": {
    "mcp-fileop": {
      "type": "stdio",
      "command": "/absolute/path/to/your/project/build/mcp_stdio",
      "description": "High-performance file operations via memory-mapped I/O"
    }
  }
}
```

Replace `/absolute/path/to/your/project` with your actual project path.

### 2. Verify Integration

1. Reload VS Code (Cmd+Shift+P → "Developer: Reload Window")
2. Open GitHub Copilot Chat
3. The server maintains state - preloaded files appear as resources
4. Try commands like:
   - "Use mcp-fileop to preload the README.md file"
   - "List all preloaded resources"
   - "Read the first 1024 bytes from the preloaded file"

### 3. Understanding Stateful Resources

When you preload a file, it:
- Gets memory-mapped for efficient access
- Appears as a resource with URI `file:///canonical/path`
- Persists until explicitly closed
- Can be read multiple times without reloading
- Sends notifications when resources list changes

## Troubleshooting

### Server Not Running
```bash
# Check if server is running
lsof -i :8080

# Or check with curl
curl http://localhost:8080/events
```

### Port Already in Use
```bash
# Find and kill process on port 8080
lsof -ti :8080 | xargs kill -9
```

### VS Code Not Detecting MCP Server
1. Ensure the server is running before opening VS Code
2. Check VS Code's Output panel (View → Output → GitHub Copilot Chat)
3. Verify the URL is correct in settings
4. Reload VS Code window

### Server Crashes
Check logs:
```bash
# If running with nohup
tail -f server.log

# Or check system logs
log show --predicate 'process == "mcp_server"' --last 1h
```

## Production Deployment

For production use, consider:

1. **systemd service** (Linux):
   ```ini
   [Unit]
   Description=MCP File Operations Server
   After=network.target

   [Service]
   Type=simple
   User=youruser
   WorkingDirectory=/path/to/mcp-fileop
   ExecStart=/path/to/mcp-fileop/build/mcp_server
   Restart=on-failure

   [Install]
   WantedBy=multi-user.target
   ```

2. **launchd service** (macOS):
   Create `~/Library/LaunchAgents/com.mcp.fileop.plist`

3. **Docker container**:
   ```dockerfile
   FROM ubuntu:22.04
   COPY build/mcp_server /usr/local/bin/
   COPY config.json /etc/mcp/
   EXPOSE 8080
   CMD ["/usr/local/bin/mcp_server"]
   ```

## API Reference

### Preload Operation
```bash
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"op": "preload", "params": {"path": "/path/to/file"}}'
```

Response:
```json
{
  "handler": "uuid-string",
  "size": 12345
}
```

### Read Operation
```bash
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"op": "read", "params": {"handler": "uuid", "offset": 0, "size": 1024, "format": "text"}}'
```

### Close Operation
```bash
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"op": "close", "params": {"handler": "uuid"}}'
```

## Security Considerations

1. **File Access Control**: Configure ROOT directory in code to restrict file access
2. **Authentication**: Add token-based auth for production
3. **TLS/HTTPS**: Enable HTTPS in config.json for secure communication
4. **Network Binding**: Change `0.0.0.0` to `127.0.0.1` in config.json for local-only access

## Performance Tips

1. Preload frequently accessed files once and reuse handlers
2. Use appropriate read chunk sizes (4KB-64KB typical)
3. Close handlers when done to free memory
4. Monitor SSE events to track server state
5. Adjust thread count in config.json based on workload

## Next Steps

- Add authentication middleware
- Implement file watching for auto-reload
- Add compression for large reads
- Implement rate limiting
- Add detailed metrics endpoint
