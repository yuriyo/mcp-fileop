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
- **Result Format**: MCP-compliant Tool Result Schema
  - Each read operation returns `content[]` array with items containing `type` and `text` fields
  - Text/lines format: `{"type": "text", "text": "..."}`
  - Hex format: `{"type": "text", "format": "hex", "text": "..."}`
  - Binary format: `{"type": "bytes", "format": "binary", "text": "..."}`
  - Multiple ranges create separate content items (no nested `parts[]`)
- **Resources**: Lists preloaded files as resources
- **Protocol**: JSON-RPC 2.0 over stdio

### HTTP API (legacy)
- `POST /mcp` - Operations: preload, read, read_multiple, close
- `GET /events` - Server-Sent Events

See `mcp_server_design.md` for full design details and `MCP_RESULT_FORMAT.md` for format specification.

## Docker (build and run)

You can build and run the `mcp_stream` service in a containerized environment using Docker or Docker Compose. The repository includes a `Dockerfile` and `docker-compose.yml` that build a release-ready `mcp_stream` image and map host directories for testing files.

### Build using Docker

Build the Docker image with a tag (the Dockerfile builds the release `mcp_stream` binary in an intermediate stage and copies it into the final image):

```bash
docker build -t mcp-fileop:latest .
```

This will create an image named `mcp-fileop:latest` using the multi-stage Dockerfile in the repo.

### Run with Docker (single container)

You can run the container directly using `docker run`. The default image exposes port 8080 and writes `config.json` using `config.docker.json` included with the repository:

```bash
docker run --rm -it -p 8080:8080 --name=mcp-fileop-server \
  -v /tmp:/mnt/tmp:ro -v $HOME/tmp:/mnt/home_tmp:ro \
  mcp-fileop:latest
```

Use `-v` volume mounts if you want the server in the container to have read access to files on the host for testing (`/tmp` and `$HOME/tmp` are used by the compose file).

### Run with Docker Compose (recommended)

To build and run the project with Docker Compose you can use the included `docker-compose.yml`. This is the easiest way to run the service with the same configuration used in development:

```bash
# Build the image and run as a background service
docker-compose build
docker-compose up -d

# Tail logs
docker-compose logs -f

# Stop and remove the containers
docker-compose down
```

By default compose maps port `8080` on the container to port `8080` on the host. The compose file also maps host paths to `/mnt/tmp` and `/mnt/home_tmp` in the container so you can `preload` files from the host.

### Access the MCP API over HTTP

Once the container is running you can use the HTTP endpoints:

```bash
# Health/Events
curl http://localhost:8080/events

# Make an MCP call (JSON-RPC) to preload or read — example (preload):
curl -s -X POST http://localhost:8080/mcp -H 'Content-Type: application/json' -d '{"op":"preload","params":{"path":"/mnt/home_tmp/myfile.bin"}}'

# Read hex (example) — format will reflect the repo's `mcp_stream` behavior
curl -s -X POST http://localhost:8080/mcp -H 'Content-Type: application/json' -d '{"op":"read","params":{"handler":"/mnt/home_tmp/myfile.bin","offset":0,"size":16,"format":"hex"}}'
```

### Notes and Tips

- The Docker image uses `config.docker.json` as `config.json` inside the container. If you need a different config, mount your own config file into `/app/config.json` when running the container.
- The current image produces `mcp_stream` (an HTTP based service). If you want to run the stdio-based server (`mcp_stdio`) locally during development, the usual way is `cmake --build build` followed by `./build/mcp_stdio` in a shell.
- The compose file sets `restart: unless-stopped`. To see logs from the running container, run `docker-compose logs -f` or `docker logs -f mcp-fileop-server`.
- If you change code, rebuild the image with `docker-compose build` or use `docker build`.

If you'd like, I can also add a simple `docker-compose.override.yml` for local development (e.g., mapping local source into the container and running the debug build). Let me know if you'd like that.

### Mounting host data into the container (so mcp can preload and read files)

The default `docker-compose.yml` mounts two host paths into the container for convenience:

- `/tmp` → `/mnt/tmp` (read-only)
- `${HOME}/tmp` → `/mnt/home_tmp` (read-only)

This means files that exist on your host under those paths will be visible in the container at `/mnt/tmp` and `/mnt/home_tmp` respectively. The `mcp_stream`/`mcp_stdio` server uses the container's canonical path as the `handler` value when you call `preload` — you should use the handler returned by `preload` (or the container path) when making subsequent `read` or `read_multiple` calls.

Example workflow for composing a file into the container and reading it via HTTP API:

1) Create a file on your host (the file will be visible as `/mnt/home_tmp` inside the container):

```bash
mkdir -p $HOME/tmp
echo "Hello, MCP from Docker" > $HOME/tmp/example.bin
```

2) Start the service:
```bash
docker-compose up -d
```

3) Preload the file via the HTTP API (use the container's path):

```bash
curl -s -X POST http://localhost:8080/mcp -H 'Content-Type: application/json' -d '{"op":"preload","params":{"path":"/mnt/home_tmp/example.bin"}}' | jq .
```

The preload call response will include a canonical `handler` in the `result.content[0].text` string and a `resourceListChanged` notification. Use that handler for subsequent read calls.

4) Read the file using the returned handler or the container path directly:

```bash
curl -s -X POST http://localhost:8080/mcp -H 'Content-Type: application/json' -d '{"op":"read","params":{"handler":"/mnt/home_tmp/example.bin","offset":0,"size":16,"format":"hex"}}' | jq .
```

Notes & troubleshooting:
- If you see `Invalid handler`, ensure you are using the handler returned by the `preload` call (canonical path) and not a host path that the container cannot see.
- The compose file uses `:ro` mounts to protect host files from modification inside the container. If you need to write to the mounted directory from within the container (uncommon for this server), remove the `:ro` suffix to mount with write access.
- On macOS, Docker uses a VM which can change some paths (for example `/tmp` may become `/private/tmp`). Using the container's `/mnt/...` path when performing operations avoids confusion with those canonical path differences.
- You may map any host path into the container (for example, `/path/to/myfiles` -> `/mnt/home_tmp:ro`) to make files accessible; update your `docker-compose.yml` or `docker run -v` options as required.
- If you want to mount a single specific file into the container, bind mount its directory and refer to the file inside the container. For example:

```bash
# on host
mkdir -p $HOME/testdata
echo 'hello' > $HOME/testdata/onefile.bin
docker run --rm -it -p 8080:8080 -v $HOME/testdata:/mnt/home_tmp:ro mcp-fileop:latest
```

### Helpful docker-compose.override.yml examples

If you are developing locally, an override can be useful to mount your working directory with data for testing:

`docker-compose.override.yml`:

```yaml
services:
  mcp-fileop:
    volumes:
      - ./testdata:/mnt/home_tmp:ro

# where ./testdata contains files you want to preload and read
```

This override keeps the image build the same but maps a local `./testdata` folder into the container where the server will read from `/mnt/home_tmp`.

