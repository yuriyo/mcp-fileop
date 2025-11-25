#include <drogon/drogon.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <memory>
#include <fstream>
#include "SegmentRegistry.hpp"
#include "SSEBroadcaster.hpp"

SegmentRegistry registry;
SSEBroadcaster broadcaster;

// JSON-RPC 2.0 response helpers
Json::Value createResponse(const Json::Value& id, const Json::Value& result) {
    Json::Value response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    return response;
}

Json::Value createError(const Json::Value& id, int code, const std::string& message) {
    Json::Value response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    return response;
}

void sendNotification(const std::string& method, const Json::Value& params = Json::Value()) {
    Json::Value notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = method;
    if (!params.isNull()) {
        notification["params"] = params;
    }
    broadcaster.broadcast(method, Json::writeString(Json::StreamWriterBuilder(), notification));
}

// Handle MCP initialize
void handleInitialize(const Json::Value& id, std::function<void(const Json::Value&)> sendResponse) {
    Json::Value result;
    result["protocolVersion"] = "2024-11-05";
    result["capabilities"]["tools"] = Json::objectValue;
    result["capabilities"]["resources"]["subscribe"] = true;
    result["capabilities"]["resources"]["listChanged"] = true;
    result["capabilities"]["streaming"] = true;
    result["serverInfo"]["name"] = "mcp-fileop-stream";
    result["serverInfo"]["version"] = "1.0.0";
    
    sendResponse(createResponse(id, result));
}

// Handle list resources
void handleListResources(const Json::Value& id, std::function<void(const Json::Value&)> sendResponse) {
    Json::Value resources(Json::arrayValue);
    
    auto handlers = registry.listHandlers();
    for (const auto& handler : handlers) {
        auto segment = registry.getByHandler(handler);
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
    
    sendResponse(createResponse(id, result));
}

// Handle read resource
void handleReadResource(const Json::Value& id, const Json::Value& params, 
                       std::function<void(const Json::Value&)> sendResponse) {
    std::string uri = params["uri"].asString();
    std::string handler = uri.substr(8); // Skip "file:///"
    
    auto segment = registry.getByHandler(handler);
    if (!segment) {
        sendResponse(createError(id, -32000, "Resource not found"));
        return;
    }
    
    const char* data = static_cast<const char*>(segment->data());
    size_t size = segment->size();
    
    Json::Value result;
    result["contents"][0]["uri"] = uri;
    result["contents"][0]["mimeType"] = "application/octet-stream";
    result["contents"][0]["text"] = std::string(data, size);
    
    sendResponse(createResponse(id, result));
}

// Handle list tools
void handleListTools(const Json::Value& id, std::function<void(const Json::Value&)> sendResponse) {
    Json::Value tools(Json::arrayValue);
    
    Json::Value fileOpTool;
    fileOpTool["name"] = "fileop";
    fileOpTool["description"] = "File operations tool supporting preload, read, stream_read, and close operations on memory-mapped files";
    fileOpTool["inputSchema"]["type"] = "object";
    
    // operation parameter
    fileOpTool["inputSchema"]["properties"]["operation"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["operation"]["description"] = "Operation to perform";
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("preload");
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("read");
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("stream_read");
    fileOpTool["inputSchema"]["properties"]["operation"]["enum"].append("close");
    
    // path parameter (for preload)
    fileOpTool["inputSchema"]["properties"]["path"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["path"]["description"] = "File path to preload (required for 'preload' operation)";
    
    // handler parameter (for read, stream_read, close)
    fileOpTool["inputSchema"]["properties"]["handler"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["handler"]["description"] = "Handler ID from preload (required for 'read', 'stream_read', 'close' operations)";
    
    // offset parameter (for read, stream_read)
    fileOpTool["inputSchema"]["properties"]["offset"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["offset"]["description"] = "Byte offset to start reading (required for 'read', 'stream_read')";
    
    // size parameter (for read, stream_read)
    fileOpTool["inputSchema"]["properties"]["size"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["size"]["description"] = "Number of bytes to read (required for 'read', 'stream_read')";
    
    // format parameter (for read, stream_read)
    fileOpTool["inputSchema"]["properties"]["format"]["type"] = "string";
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("binary");
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("hex");
    fileOpTool["inputSchema"]["properties"]["format"]["enum"].append("text");
    fileOpTool["inputSchema"]["properties"]["format"]["description"] = "Output format (optional for 'read', 'stream_read', default: 'text')";
    fileOpTool["inputSchema"]["properties"]["format"]["default"] = "text";
    
    // chunk_size parameter (for stream_read)
    fileOpTool["inputSchema"]["properties"]["chunk_size"]["type"] = "number";
    fileOpTool["inputSchema"]["properties"]["chunk_size"]["description"] = "Size of each chunk (optional for 'stream_read', default: 65536)";
    fileOpTool["inputSchema"]["properties"]["chunk_size"]["default"] = 65536;
    
    // Required fields
    fileOpTool["inputSchema"]["required"].append("operation");
    
    tools.append(fileOpTool);
    
    Json::Value result;
    result["tools"] = tools;
    
    sendResponse(createResponse(id, result));
}

// Handle tool calls
void handleCallTool(const Json::Value& id, const Json::Value& params,
                   std::function<void(const Json::Value&)> sendResponse,
                   std::function<void(const Json::Value&)> sendProgress) {
    std::string toolName = params["name"].asString();
    Json::Value arguments = params["arguments"];
    Json::Value result;
    
    try {
        if (toolName != "fileop") {
            sendResponse(createError(id, -32601, "Unknown tool: " + toolName));
            return;
        }
        
        std::string operation = arguments["operation"].asString();
        
        if (operation == "preload") {
            std::string path = arguments["path"].asString();
            auto segment = registry.preload(path);
            if (segment) {
                std::filesystem::path canonical_path = std::filesystem::canonical(path);
                std::string handler = canonical_path.string();
                
                result["content"][0]["type"] = "text";
                result["content"][0]["text"] = "File preloaded successfully.\n\nHandler: " + handler + 
                                               "\nSize: " + std::to_string(segment->size()) + " bytes" +
                                               "\nResource URI: file:///" + handler;
                
                sendResponse(createResponse(id, result));
                sendNotification("notifications/resources/list_changed");
            } else {
                sendResponse(createError(id, -32000, "Failed to preload file"));
            }
        } else if (operation == "read") {
            std::string handler = arguments["handler"].asString();
            size_t offset = arguments["offset"].asUInt64();
            size_t size = arguments["size"].asUInt64();
            std::string format = arguments.get("format", "text").asString();
            
            auto segment = registry.getByHandler(handler);
            if (!segment) {
                sendResponse(createError(id, -32000, "Invalid handler"));
                return;
            }
            if (offset + size > segment->size()) {
                sendResponse(createError(id, -32000, "Read out of bounds"));
                return;
            }
            
            const char* data = static_cast<const char*>(segment->data()) + offset;
            std::string content;
            
            if (format == "hex") {
                std::stringstream ss;
                for (size_t i = 0; i < size; ++i) {
                    ss << std::hex << std::setw(2) << std::setfill('0') 
                       << (unsigned int)(unsigned char)data[i];
                }
                content = ss.str();
            } else {
                content = std::string(data, size);
            }
            
            result["content"][0]["type"] = "text";
            result["content"][0]["text"] = content;
            sendResponse(createResponse(id, result));
            
        } else if (operation == "stream_read") {
            std::string handler = arguments["handler"].asString();
            size_t offset = arguments["offset"].asUInt64();
            size_t total_size = arguments["size"].asUInt64();
            size_t chunk_size = arguments.get("chunk_size", 65536).asUInt64();
            std::string format = arguments.get("format", "text").asString();
            
            auto segment = registry.getByHandler(handler);
            if (!segment) {
                sendResponse(createError(id, -32000, "Invalid handler"));
                return;
            }
            if (offset + total_size > segment->size()) {
                sendResponse(createError(id, -32000, "Read out of bounds"));
                return;
            }
            
            // Stream chunks with progress
            size_t remaining = total_size;
            size_t current_offset = offset;
            std::stringstream full_content;
            
            while (remaining > 0) {
                size_t current_chunk = std::min(chunk_size, remaining);
                const char* data = static_cast<const char*>(segment->data()) + current_offset;
                
                if (format == "hex") {
                    for (size_t i = 0; i < current_chunk; ++i) {
                        full_content << std::hex << std::setw(2) << std::setfill('0') 
                                    << (unsigned int)(unsigned char)data[i];
                    }
                } else {
                    full_content << std::string(data, current_chunk);
                }
                
                // Send progress notification
                Json::Value progress;
                progress["bytes_read"] = (Json::Value::UInt64)(total_size - remaining + current_chunk);
                progress["total_bytes"] = (Json::Value::UInt64)total_size;
                progress["progress"] = (double)(total_size - remaining + current_chunk) / total_size;
                sendProgress(progress);
                
                current_offset += current_chunk;
                remaining -= current_chunk;
            }
            
            result["content"][0]["type"] = "text";
            result["content"][0]["text"] = full_content.str();
            sendResponse(createResponse(id, result));
            
        } else if (operation == "close") {
            std::string handler = arguments["handler"].asString();
            registry.close(handler);
            
            result["content"][0]["type"] = "text";
            result["content"][0]["text"] = "Handler closed successfully: " + handler;
            
            sendResponse(createResponse(id, result));
            sendNotification("notifications/resources/list_changed");
        } else {
            sendResponse(createError(id, -32601, "Unknown operation: " + operation));
        }
    } catch (const std::exception& e) {
        sendResponse(createError(id, -32000, std::string("Error: ") + e.what()));
    }
}

// Main HTTP handler for MCP JSON-RPC requests
void handleMcpRequest(const drogon::HttpRequestPtr& req, 
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            createError(Json::Value::null, -32700, "Parse error"));
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string method = (*json)["method"].asString();
    Json::Value id = (*json)["id"];
    Json::Value params = (*json)["params"];
    
    auto sendResponse = [callback](const Json::Value& response) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);
    };
    
    auto sendProgress = [](const Json::Value& progress) {
        // For non-streaming HTTP, we can't send intermediate progress
        // Progress would be sent via SSE in a real streaming scenario
    };
    
    if (method == "initialize") {
        handleInitialize(id, sendResponse);
    } else if (method == "tools/list") {
        handleListTools(id, sendResponse);
    } else if (method == "tools/call") {
        handleCallTool(id, params, sendResponse, sendProgress);
    } else if (method == "resources/list") {
        handleListResources(id, sendResponse);
    } else if (method == "resources/read") {
        handleReadResource(id, params, sendResponse);
    } else if (method == "notifications/initialized") {
        // No response for notifications
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k204NoContent);
        callback(resp);
    } else {
        sendResponse(createError(id, -32601, "Method not found: " + method));
    }
}

// SSE endpoint for streaming notifications and progress
void handleSSE(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Create SSE response
    auto resp = drogon::HttpResponse::newStreamResponse(
        [](char* buffer, std::size_t len) -> std::size_t {
            // Initial connection message
            std::string msg = "data: {\"type\":\"connected\"}\n\n";
            if (msg.size() <= len) {
                memcpy(buffer, msg.c_str(), msg.size());
                return msg.size();
            }
            return 0;
        },
        "event-stream"
    );
    
    resp->addHeader("Cache-Control", "no-cache");
    resp->addHeader("Connection", "keep-alive");
    resp->addHeader("X-Accel-Buffering", "no");
    
    callback(resp);
    
    // Register connection with broadcaster
    // In a real implementation, we'd need to manage the connection lifecycle
}

// WebSocket endpoint for full duplex streaming - using HTTP upgrade
void handleWebSocketUpgrade(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // For WebSocket support, we'll use SSE as a simpler alternative
    // Full WebSocket implementation requires Drogon's WebSocket controller pattern
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::HttpStatusCode::k501NotImplemented);
    resp->setBody("WebSocket endpoint - use /mcp/events for SSE streaming");
    callback(resp);
}

int main() {
    using namespace drogon;
    
    // Load config and set allowed paths
    app().loadConfigFile("config.json");
    
    // Read config file directly for custom mcp section
    try {
        std::ifstream configFile("config.json");
        Json::Value config;
        Json::CharReaderBuilder builder;
        std::string errs;
        
        if (Json::parseFromStream(builder, configFile, &config, &errs)) {
            std::cout << "Config file parsed successfully" << std::endl;
            
            if (config.isMember("mcp")) {
                auto mcpConfig = config["mcp"];
                std::cout << "MCP config section found" << std::endl;
                
                if (mcpConfig.isMember("allowed_paths")) {
                    std::vector<std::string> allowedPaths;
                    for (const auto& path : mcpConfig["allowed_paths"]) {
                        std::string pathStr = path.asString();
                        allowedPaths.push_back(pathStr);
                        std::cout << "  - Adding allowed path: " << pathStr << std::endl;
                    }
                    registry.setAllowedPaths(allowedPaths);
                    std::cout << "Configured " << allowedPaths.size() << " allowed path(s)" << std::endl;
                } else {
                    std::cout << "No 'allowed_paths' key in mcp config" << std::endl;
                }
            } else {
                std::cout << "No 'mcp' section in config" << std::endl;
            }
        } else {
            std::cout << "Failed to parse config.json: " << errs << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error loading path config: " << e.what() << std::endl;
        std::cout << "No path restrictions configured (all paths allowed)" << std::endl;
    } catch (...) {
        std::cout << "Unknown error loading path config" << std::endl;
        std::cout << "No path restrictions configured (all paths allowed)" << std::endl;
    }
    
    // HTTP JSON-RPC endpoint
    app().registerHandler("/mcp",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            handleMcpRequest(req, std::move(callback));
        },
        {Post});
    
    // SSE endpoint for notifications
    app().registerHandler("/mcp/events",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            handleSSE(req, std::move(callback));
        },
        {Get});
    
    // WebSocket endpoint placeholder
    app().registerHandler("/mcp/ws",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            handleWebSocketUpgrade(req, std::move(callback));
        },
        {Get});
    
    // CORS support
    app().registerPreHandlingAdvice([](const HttpRequestPtr& req) -> HttpResponsePtr {
        if (req->method() == Options) {
            auto resp = HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
            return resp;
        }
        return nullptr;
    });
    
    app().registerPostHandlingAdvice([](const HttpRequestPtr&, const HttpResponsePtr& resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
    });
    
    // Get configured port
    int port = 8080;
    try {
        auto listeners = app().getCustomConfig()["listeners"];
        if (!listeners.empty() && listeners[0].isMember("port")) {
            port = listeners[0]["port"].asInt();
        }
    } catch (...) {
        // Use default port if config parsing fails
    }
    
    std::cout << "MCP Stream Server starting on port " << port << std::endl;
    std::cout << "  HTTP endpoint: http://localhost:" << port << "/mcp" << std::endl;
    std::cout << "  SSE endpoint: http://localhost:" << port << "/mcp/events" << std::endl;
    std::cout << "  WebSocket endpoint: ws://localhost:" << port << "/mcp/ws" << std::endl;
    app().run();
    
    return 0;
}
