#include "FileOpController.hpp"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <vector>

FileOpController::FileOpController() {
}

Json::Value FileOpController::createResponse(const Json::Value& id, const Json::Value& result) const {
    Json::Value response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    return response;
}

Json::Value FileOpController::createError(const Json::Value& id, int code, const std::string& message) const {
    Json::Value response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    return response;
}

Json::Value FileOpController::listTools() const {
    Json::Value tools(Json::arrayValue);

    Json::Value fileOpTool;
    fileOpTool["name"] = "fileop";
    fileOpTool["description"] = "File operations tool supporting preload, read, read_multiple, and close operations on memory-mapped files";
    fileOpTool["inputSchema"]["type"] = "object";

    // operation parameter
    fileOpTool["inputSchema"]["properties"]["operation"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["operation"]["description"] = "Operation to perform";
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("preload");
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("read");
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("read_multiple");
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("close");

    // path parameter (for preload)
    fileOpTool["inputSchema"]["properties"]["path"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["path"]["description"] = "File path to preload (required for 'preload' operation)";

    // handler parameter (for read, close)
    fileOpTool["inputSchema"]["properties"]["handler"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["handler"]["description"] = "Handler ID from preload (required for 'read', 'close' operations)";

    // offset parameter (for read)
    fileOpTool["inputSchema"]["properties"]["offset"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["offset"]["description"] = "Starting position to read from (required for 'read'). For 'lines' format: zero-based line number. For all other formats: byte offset.";

    // size parameter (for read)
    fileOpTool["inputSchema"]["properties"]["size"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["size"]["description"] = "Amount to read (required for 'read'). For 'lines' format: number of lines to read. For all other formats: number of bytes to read.";

    // format parameter (for read, stream_read)
    fileOpTool["inputSchema"]["properties"]["format"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("binary");
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("hex");
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("text");
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("lines");
    fileOpTool["inputSchema"]["properties"]["format"]["description"] = "Output format (optional for 'read' and 'read_multiple', default: 'text'). When format is 'lines', offset/size parameters are interpreted as line numbers/counts instead of byte offsets/sizes.";
    fileOpTool["inputSchema"]["properties"]["format"]["default"] = "text";

    // Deprecated: chunk_size was used for stream_read; not supported anymore.
    
    // segments parameter (for read_multiple) - array of { handler, format?, ranges: [{offset,size}] }
    fileOpTool["inputSchema"]["properties"]["segments"]["type"] = "array";
    fileOpTool["inputSchema"]["properties"]["segments"]["description"] = "Array of segments to read. Each segment contains 'handler', optional 'format', and 'ranges' specifying offsets and sizes.";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["type"] = "object";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["handler"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["format"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["format"]["enum"].append("binary");
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["format"]["enum"].append("hex");
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["format"]["enum"].append("text");
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["format"]["enum"].append("lines");
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["ranges"]["type"] = "array";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["ranges"]["items"]["type"] = "object";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["ranges"]["items"]["properties"]["offset"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["offset"]["description"] = "Starting position. For 'lines' format: zero-based line number. For other formats: byte offset.";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["ranges"]["items"]["properties"]["size"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["segments"]["items"]["properties"]["size"]["description"] = "Amount to read. For 'lines' format: number of lines. For other formats: number of bytes.";

    // Required fields
    fileOpTool["inputSchema"]["required"].append("operation");

    tools.append(fileOpTool);
    Json::Value result;
    result["tools"] = tools;
    return result;
}

Json::Value FileOpController::listResources() {
    Json::Value resources(Json::arrayValue);
    auto handlers = registry_.listHandlers();
    for (const auto& handler : handlers) {
        auto segment = registry_.getByHandler(handler);
        if (segment) {
            Json::Value resource;
            resource["uri"] = "file:///" + handler;
            resource["name"] = std::filesystem::path(handler).filename().string();
            resource["description"] = "Memory-mapped file (" + std::to_string(segment->size()) + " bytes)";
            resource["mimeType"] = "application/octet-stream";
            resources.append(resource);
        }
    }
    Json::Value result;
    result["resources"] = resources;
    return result;
}

Json::Value FileOpController::readResourceFromUri(const Json::Value& params) {
    Json::Value result;
    std::string uri = params["uri"].asString();
    std::string handler = uri.substr(8); // Skip file:/// prefix
    auto segment = registry_.getByHandler(handler);
    if (!segment) {
        result["__error__"] = "Resource not found";
        return result;
    }
    const char* data = static_cast<const char*>(segment->data());
    size_t size = segment->size();
    result["contents"][0]["uri"] = uri;
    result["contents"][0]["mimeType"] = "application/octet-stream";
    result["contents"][0]["text"] = std::string(data, size);
    return result;
}

// 'lines' helper now centralized in LineUtils.hpp
#include "LineUtils.hpp"

Json::Value FileOpController::callTool(const Json::Value& params, std::function<void(const Json::Value&)> progress) {
    Json::Value result;
    std::string toolName = params["name"].asString();
    Json::Value arguments = params["arguments"];
    // Compatibility: accept 'preload', 'read', 'close' as top-level tool names (stdio variant)
    if (toolName != "fileop") {
        if (toolName == "preload") {
            Json::Value compat;
            compat["name"] = "fileop";
            compat["arguments"]["operation"] = "preload";
            compat["arguments"]["path"] = arguments.get("path", Json::Value());
            toolName = "fileop";
            arguments = compat["arguments"];
        } else if (toolName == "read") {
            Json::Value compat;
            compat["name"] = "fileop";
            compat["arguments"]["operation"] = "read";
            compat["arguments"]["handler"] = arguments.get("handler", Json::Value());
            compat["arguments"]["offset"] = arguments.get("offset", Json::Value());
            compat["arguments"]["size"] = arguments.get("size", Json::Value());
            compat["arguments"]["format"] = arguments.get("format", Json::Value("text"));
            toolName = "fileop";
            arguments = compat["arguments"];
        } else if (toolName == "close") {
            Json::Value compat;
            compat["name"] = "fileop";
            compat["arguments"]["operation"] = "close";
            compat["arguments"]["handler"] = arguments.get("handler", Json::Value());
            toolName = "fileop";
            arguments = compat["arguments"];
        }
        else if (toolName == "read_multiple") {
            Json::Value compat;
            compat["name"] = "fileop";
            compat["arguments"]["operation"] = "read_multiple";
            compat["arguments"]["segments"] = arguments.get("segments", Json::Value());
            toolName = "fileop";
            arguments = compat["arguments"];
        }
    }
    try {
        if (toolName != "fileop") {
            result["__error__"] = std::string("Unknown tool: ") + toolName;
            return result;
        }

        std::string operation = arguments["operation"].asString();
        // Normalize single-range read into read_multiple to keep a single implementation
        if (operation == "read") {
            // extract read parameters and rebuild arguments to use read_multiple
            std::string handler = arguments["handler"].asString();
            size_t offset = arguments["offset"].asUInt64();
            size_t size = arguments["size"].asUInt64();
            std::string format = arguments.get("format", "text").asString();
            Json::Value compat;
            compat["arguments"]["segments"] = Json::Value(Json::arrayValue);
            Json::Value seg;
            seg["handler"] = handler;
            seg["format"] = format;
            seg["ranges"][0]["offset"] = (Json::Value::UInt64)offset;
            seg["ranges"][0]["size"] = (Json::Value::UInt64)size;
            compat["arguments"]["segments"].append(seg);
            arguments = compat["arguments"];
            operation = "read_multiple";
        }
        if (operation == "preload") {
            std::string path = arguments["path"].asString();
            auto segment = registry_.preload(path);
            if (segment) {
                std::filesystem::path canonical_path = std::filesystem::canonical(path);
                std::string handler = canonical_path.string();
                result["content"][0]["type"] = "text";
                result["content"][0]["text"] = "File preloaded successfully.\n\nHandler: " + handler + "\nSize: " + std::to_string(segment->size()) + " bytes" + "\nResource URI: file:///" + handler;
                result["resourceListChanged"] = true;
                return result;
            } else {
                result["__error__"] = "Failed to preload file";
                return result;
            }
        // `read` now normalizes to `read_multiple` above and continues to the `read_multiple` branch
        // stream_read removed: use read or read_multiple instead
        } else if (operation == "read_multiple") {
            // `segments` is an array of objects { handler, format?, ranges: [{offset, size}, ...] }
            if (!arguments.isMember("segments") || !arguments["segments"].isArray()) {
                result["__error__"] = "segments must be an array";
                return result;
            }
            // Precompute total bytes for progress
            uint64_t total_bytes = 0;
            for (const auto& s : arguments["segments"]) {
                std::string handler = s["handler"].asString();
                std::string format = s.get("format", Json::Value("text")).asString();
                auto segment = registry_.getByHandler(handler);
                if (!segment) {
                    result["__error__"] = std::string("Invalid handler: ") + handler;
                    return result;
                }
                const char* data = static_cast<const char*>(segment->data());
                size_t seg_size = segment->size();
                for (const auto& r : s["ranges"]) {
                    if (format == "lines") {
                        size_t start_byte = 0;
                        size_t bytes_len = 0;
                        size_t start_line = r["offset"].asUInt64();
                        size_t max_lines = r["size"].asUInt64();
                        if (!compute_line_byte_range(data, seg_size, start_line, max_lines, start_byte, bytes_len)) {
                            result["__error__"] = std::string("Read out of bounds for handler (lines): ") + handler;
                            return result;
                        }
                        total_bytes += (Json::UInt64)bytes_len;
                    } else {
                        total_bytes += (Json::UInt64)r["size"].asUInt64();
                    }
                }
            }
            uint64_t bytes_so_far = 0;

            // Build result contents conforming to MCP Tool Result Schema
            Json::Value content_array(Json::arrayValue);
            
            for (const auto& s : arguments["segments"]) {
                std::string handler = s["handler"].asString();
                std::string format = s.get("format", Json::Value("text")).asString();
                auto segment = registry_.getByHandler(handler);
                if (!segment) {
                    result["__error__"] = std::string("Invalid handler: ") + handler;
                    return result;
                }
                
                for (const auto& r : s["ranges"]) {
                    size_t offset = r["offset"].asUInt64();
                    size_t size = r["size"].asUInt64();
                    std::string content;
                    size_t actual_bytes = 0;
                    
                    if (format == "lines") {
                        size_t start_byte = 0;
                        size_t bytes_len = 0;
                        if (!compute_line_byte_range(static_cast<const char*>(segment->data()), segment->size(), offset, size, start_byte, bytes_len)) {
                            result["__error__"] = std::string("Read out of bounds for handler: ") + handler;
                            return result;
                        }
                        const char* data = static_cast<const char*>(segment->data()) + start_byte;
                        if (bytes_len > 0) content = std::string(data, bytes_len);
                        actual_bytes = bytes_len;
                    } else {
                        if (offset + size > segment->size()) {
                            result["__error__"] = std::string("Read out of bounds for handler: ") + handler;
                            return result;
                        }
                        const char* data = static_cast<const char*>(segment->data()) + offset;
                        if (format == "hex") {
                            std::stringstream ss;
                            for (size_t i = 0; i < size; ++i) {
                                ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)(unsigned char)data[i];
                            }
                            content = ss.str();
                            actual_bytes = size;
                        } else {
                            content = std::string(data, size);
                            actual_bytes = size;
                        }
                    }
                    
                    // Create MCP-compliant content item
                    Json::Value content_item;
                    // For hex output, return it as text so it conforms with MCP tool schema
                    // (type: "text", text: "...").
                    // Binary output handling remains as-is for now.
                    if (format == "hex") {
                        content_item["type"] = "text";
                        content_item["format"] = "hex";
                    } else if (format == "binary") {
                        content_item["type"] = "bytes";
                        content_item["format"] = "binary";
                    } else {
                        content_item["type"] = "text";
                    }
                    content_item["text"] = content;
                    content_array.append(content_item);

                    bytes_so_far += actual_bytes;
                    if (progress) {
                        Json::Value p;
                        p["bytes_read"] = (Json::Value::UInt64)bytes_so_far;
                        p["total_bytes"] = (Json::Value::UInt64)total_bytes;
                        p["progress"] = (double)bytes_so_far / (double)total_bytes;
                        progress(p);
                    }
                }
            }
            
            // Wrap in MCP Tool Result Schema format
            result["content"] = content_array;
            return result;
        } else if (operation == "close") {
            std::string handler = arguments["handler"].asString();
            registry_.close(handler);
            result["content"][0]["type"] = "text";
            result["content"][0]["text"] = std::string("Handler closed successfully: ") + handler;
            result["resourceListChanged"] = true;
            return result;
        } else {
            result["__error__"] = std::string("Unknown operation: ") + operation;
            return result;
        }
    } catch (const std::exception& e) {
        result["__error__"] = std::string("Error: ") + e.what();
        return result;
    }
}

void FileOpController::setAllowedPaths(const std::vector<std::string>& paths) {
    registry_.setAllowedPaths(paths);
}
