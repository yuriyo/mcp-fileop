# MCP File Operations - Streaming Server

This document describes the streaming MCP server implementation that supports multiple transport protocols.

## Server Implementations

### 1. `mcp_stdio` - Standard I/O Transport
- **Protocol**: JSON-RPC 2.0 over stdin/stdout
- **Use Case**: VS Code extensions, CLI tools
- **Features**: Simple request/response, notifications
- **Build**: `cmake --build build --target mcp_stdio`
- **Run**: `./build/mcp_stdio`

### 2. `mcp_server` - HTTP Transport
- **Protocol**: JSON-RPC 2.0 over HTTP
- **Use Case**: Web applications, REST clients
- **Features**: Simple HTTP POST requests
- **Build**: `cmake --build build --target mcp_server`
- **Run**: `./build/mcp_server`
- **Endpoint**: `http://localhost:8080/mcp`

### 3. `mcp_stream` - Streaming Transport (NEW)
- **Protocol**: JSON-RPC 2.0 over HTTP/SSE/WebSocket
- **Use Case**: Real-time applications, progress tracking, large file operations
- **Features**: 
  - HTTP endpoint for regular requests
  - SSE for server-to-client notifications
  - WebSocket for full-duplex streaming with progress updates
- **Build**: `cmake --build build --target mcp_stream`
- **Run**: `./build/mcp_stream`
- **Endpoints**:
  - HTTP: `http://localhost:8080/mcp`
  - SSE: `http://localhost:8080/mcp/events`
  - WebSocket: `ws://localhost:8080/mcp/ws`

## Streaming Features

### Server-Sent Events (SSE)
The SSE endpoint provides server-to-client notifications:
- Connection status
- Resource list changes
- Tool execution notifications

```javascript
const eventSource = new EventSource('http://localhost:8080/mcp/events');
eventSource.onmessage = (event) => {
    const data = JSON.parse(event.data);
    console.log('Notification:', data);
};
```

### WebSocket Full-Duplex Streaming
The WebSocket endpoint enables bidirectional communication with progress updates:

```javascript
const ws = new WebSocket('ws://localhost:8080/mcp/ws');

ws.onopen = () => {
    // Send initialize request
    ws.send(JSON.stringify({
        jsonrpc: "2.0",
        id: 1,
        method: "initialize",
        params: { capabilities: {} }
    }));
};

ws.onmessage = (event) => {
    const response = JSON.parse(event.data);
    
    if (response.method === "notifications/progress") {
        // Handle progress notification
        console.log('Progress:', response.params.value);
    } else if (response.result) {
        // Handle result
        console.log('Result:', response.result);
    }
};
```

### Stream Read Tool
The new `stream_read` tool provides chunked reading with progress updates:

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/call",
    "params": {
        "name": "stream_read",
        "arguments": {
            "handler": "/path/to/file",
            "offset": 0,
            "size": 10485760,
            "chunk_size": 65536,
            "format": "hex"
        }
    }
}
```

Progress notifications will be sent during the read operation:
```json
{
    "jsonrpc": "2.0",
    "method": "notifications/progress",
    "params": {
        "progressToken": 1,
        "value": {
            "bytes_read": 65536,
            "total_bytes": 10485760,
            "progress": 0.00625
        }
    }
}
```

## Available Tools

All three servers support these tools:

### 1. preload
Memory-map a file for efficient reading.
```json
{
    "method": "tools/call",
    "params": {
        "name": "preload",
        "arguments": {
            "path": "/path/to/file"
        }
    }
}
```

### 2. read
Read a specific range of bytes.
```json
{
    "method": "tools/call",
    "params": {
        "name": "read",
        "arguments": {
            "handler": "/path/to/file",
            "offset": 0,
            "size": 1024,
            "format": "hex"
        }
    }
}
```

### 3. stream_read (mcp_stream only)
Stream read with progress updates.
```json
{
    "method": "tools/call",
    "params": {
        "name": "stream_read",
        "arguments": {
            "handler": "/path/to/file",
            "offset": 0,
            "size": 10485760,
            "chunk_size": 65536,
            "format": "hex"
        }
    }
}
```

### 4. close
Close and unmap a file handler.
```json
{
    "method": "tools/call",
    "params": {
        "name": "close",
        "arguments": {
            "handler": "/path/to/file"
        }
    }
}
```

## Resource Management

All servers expose preloaded files as MCP resources:

```json
{
    "method": "resources/list"
}
```

Response:
```json
{
    "resources": [
        {
            "uri": "file:///path/to/file",
            "name": "file",
            "description": "Memory-mapped file (125238407 bytes)",
            "mimeType": "application/octet-stream"
        }
    ]
}
```

## Notifications

The streaming server sends notifications for:

### 1. Resource List Changes
Sent when files are preloaded or closed:
```json
{
    "jsonrpc": "2.0",
    "method": "notifications/resources/list_changed"
}
```

### 2. Progress Updates (WebSocket only)
Sent during streaming operations:
```json
{
    "jsonrpc": "2.0",
    "method": "notifications/progress",
    "params": {
        "progressToken": <request_id>,
        "value": {
            "bytes_read": 65536,
            "total_bytes": 10485760,
            "progress": 0.00625
        }
    }
}
```

## Comparison

| Feature | stdio | HTTP | Stream |
|---------|-------|------|--------|
| Transport | stdin/stdout | HTTP | HTTP/SSE/WS |
| Request/Response | ✓ | ✓ | ✓ |
| Notifications | ✓ | ✗ | ✓ |
| Progress Updates | ✗ | ✗ | ✓ (WS only) |
| Streaming | ✗ | ✗ | ✓ |
| Web Browser | ✗ | ✓ | ✓ |
| VS Code | ✓ | ✗ | ✗ |
| CORS | N/A | ✗ | ✓ |

## Building and Running

```bash
# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

# Build all servers
cmake --build build

# Run specific server
./build/mcp_stdio      # For VS Code/CLI
./build/mcp_server     # For HTTP clients
./build/mcp_stream     # For streaming clients
```

## Use Cases

- **mcp_stdio**: VS Code extensions, command-line tools, scripts
- **mcp_server**: Simple web integrations, API testing, basic automation
- **mcp_stream**: Large file processing, real-time monitoring, web dashboards, progress tracking

## Example: Processing Large Files

Using the streaming server with WebSocket for a large file:

```javascript
const ws = new WebSocket('ws://localhost:8080/mcp/ws');

ws.onopen = async () => {
    // 1. Preload large file
    ws.send(JSON.stringify({
        jsonrpc: "2.0",
        id: 1,
        method: "tools/call",
        params: {
            name: "preload",
            arguments: { path: "/path/to/large/file.zip" }
        }
    }));
};

ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    
    if (msg.id === 1 && msg.result) {
        // 2. Start streaming read
        const handler = msg.result.content[0].text.match(/Handler: (.+)/)[1];
        ws.send(JSON.stringify({
            jsonrpc: "2.0",
            id: 2,
            method: "tools/call",
            params: {
                name: "stream_read",
                arguments: {
                    handler: handler,
                    offset: 0,
                    size: 125238407,
                    chunk_size: 65536,
                    format: "hex"
                }
            }
        }));
    } else if (msg.method === "notifications/progress") {
        // 3. Display progress
        const progress = msg.params.value.progress * 100;
        console.log(`Progress: ${progress.toFixed(2)}%`);
    } else if (msg.id === 2 && msg.result) {
        // 4. Processing complete
        console.log('Read complete, got', msg.result.content[0].text.length, 'bytes');
    }
};
```
