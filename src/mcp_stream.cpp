#include <drogon/drogon.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <memory>
#include <fstream>
#include "SegmentRegistry.hpp" // included for historical reasons; registry now encapsulated in FileOpController
#include "SSEBroadcaster.hpp"
#include "FileOpController.hpp"

FileOpController controller;
SSEBroadcaster broadcaster;

// JSON-RPC 2.0 response helpers
// Use the controller helpers
inline Json::Value createResponse(const Json::Value& id, const Json::Value& result) { return controller.createResponse(id, result); }
inline Json::Value createError(const Json::Value& id, int code, const std::string& message) { return controller.createError(id, code, message); }

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
    Json::Value result = controller.listResources();
    sendResponse(createResponse(id, result));
}

// Handle read resource
void handleReadResource(const Json::Value& id, const Json::Value& params, std::function<void(const Json::Value&)> sendResponse) {
    Json::Value result = controller.readResourceFromUri(params);
    if (result.isMember("__error__")) {
        sendResponse(createError(id, -32000, result["__error__"].asString()));
        return;
    }
    sendResponse(createResponse(id, result));
}

// Handle list tools
void handleListTools(const Json::Value& id, std::function<void(const Json::Value&)> sendResponse) {
    Json::Value result = controller.listTools();
    sendResponse(createResponse(id, result));
}

// Handle tool calls
void handleCallTool(const Json::Value& id, const Json::Value& params, std::function<void(const Json::Value&)> sendResponse, std::function<void(const Json::Value&)> sendProgress) {
    // Forward to controller with a progress callback that uses the stream's progress sender.
    auto progressCb = [sendProgress](const Json::Value& p) {
        if (sendProgress) sendProgress(p);
    };
    Json::Value result = controller.callTool(params, progressCb);
    if (result.isMember("__error__")) {
        sendResponse(createError(id, -32000, result["__error__"].asString()));
        return;
    }
    sendResponse(createResponse(id, result));
    if (result.get("resourceListChanged", false).asBool()) {
        sendNotification("notifications/resources/list_changed");
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
                    controller.setAllowedPaths(allowedPaths);
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
