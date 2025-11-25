# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    libssl-dev \
    uuid-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Clone and bootstrap vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /build/vcpkg && \
    cd /build/vcpkg && \
    ./bootstrap-vcpkg.sh

# Copy source code
COPY CMakeLists.txt /build/
COPY src/ /build/src/
COPY tests/ /build/tests/

# Install vcpkg dependencies (only what mcp_stream needs)
RUN /build/vcpkg/vcpkg install drogon boost-system boost-filesystem boost-interprocess jsoncpp

# Build the project
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/build/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DBUILD_TESTS=OFF
RUN cmake --build build --target mcp_stream --config Release

# Runtime stage - minimal image
FROM ubuntu:22.04

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    uuid-runtime \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Create app directory and mount point
WORKDIR /app
RUN mkdir -p /mnt

# Copy binary and config from builder
COPY --from=builder /build/build/mcp_stream /app/
COPY config.docker.json /app/config.json

# Expose port
EXPOSE 8080

# Run the server
CMD ["./mcp_stream"]
