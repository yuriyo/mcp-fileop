#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <json/json.h>
#include "SegmentRegistry.hpp"

SegmentRegistry registry;

// JSON-RPC 2.0 response helper
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

void handleInitialize(const Json::Value& id) {
    Json::Value result;
    result["protocolVersion"] = "2024-11-05";
    result["capabilities"]["tools"] = Json::objectValue;
    result["capabilities"]["resources"]["subscribe"] = false;
    result["capabilities"]["resources"]["listChanged"] = true;
    result["serverInfo"]["name"] = "mcp-fileop";
    result["serverInfo"]["version"] = "1.0.0";
    
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";  // Compact output
    std::cout << Json::writeString(writer, response) << std::endl;
}

void handleListResources(const Json::Value& id) {
    Json::Value resources(Json::arrayValue);
    
    // Get all preloaded files from registry
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
    
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";  // Compact output
    std::cout << Json::writeString(writer, response) << std::endl;
}

void handleReadResource(const Json::Value& id, const Json::Value& params) {
    std::string uri = params["uri"].asString();
    
    // Extract handler from URI (file:///path -> /path)
    std::string handler = uri.substr(8); // Skip "file:///"
    
    auto segment = registry.getByHandler(handler);
    if (!segment) {
        Json::Value response = createError(id, -32000, "Resource not found");
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::cout << Json::writeString(writer, response) << std::endl;
        return;
    }
    
    // Read entire file content
    const char* data = static_cast<const char*>(segment->data());
    size_t size = segment->size();
    
    Json::Value result;
    result["contents"][0]["uri"] = uri;
    result["contents"][0]["mimeType"] = "application/octet-stream";
    result["contents"][0]["text"] = std::string(data, size);
    
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::cout << Json::writeString(writer, response) << std::endl;
}

void handleListTools(const Json::Value& id) {
    Json::Value tools(Json::arrayValue);
    
    Json::Value preloadTool;
    preloadTool["name"] = "preload";
    preloadTool["description"] = "Memory-map a file for efficient reading. The file becomes available as a resource.";
    preloadTool["inputSchema"]["type"] = "object";
    preloadTool["inputSchema"]["properties"]["path"]["type"] = "string";
    preloadTool["inputSchema"]["properties"]["path"]["description"] = "File path to preload";
    preloadTool["inputSchema"]["required"].append("path");
    tools.append(preloadTool);
    
    Json::Value readTool;
    readTool["name"] = "read";
    readTool["description"] = "Read a specific range of bytes from a preloaded file";
    readTool["inputSchema"]["type"] = "object";
    readTool["inputSchema"]["properties"]["handler"]["type"] = "string";
    readTool["inputSchema"]["properties"]["handler"]["description"] = "Handler ID from preload (canonical file path)";
    readTool["inputSchema"]["properties"]["offset"]["type"] = "number";
    readTool["inputSchema"]["properties"]["offset"]["description"] = "Byte offset to start reading";
    readTool["inputSchema"]["properties"]["size"]["type"] = "number";
    readTool["inputSchema"]["properties"]["size"]["description"] = "Number of bytes to read";
    readTool["inputSchema"]["properties"]["format"]["type"] = "string";
    readTool["inputSchema"]["properties"]["format"]["enum"].append("binary");
    readTool["inputSchema"]["properties"]["format"]["enum"].append("hex");
    readTool["inputSchema"]["properties"]["format"]["enum"].append("text");
    readTool["inputSchema"]["properties"]["format"]["description"] = "Output format";
    readTool["inputSchema"]["properties"]["format"]["default"] = "text";
    readTool["inputSchema"]["required"].append("handler");
    readTool["inputSchema"]["required"].append("offset");
    readTool["inputSchema"]["required"].append("size");
    tools.append(readTool);
    
    Json::Value closeTool;
    closeTool["name"] = "close";
    closeTool["description"] = "Close and unmap a file handler, removing it from resources";
    closeTool["inputSchema"]["type"] = "object";
    closeTool["inputSchema"]["properties"]["handler"]["type"] = "string";
    closeTool["inputSchema"]["properties"]["handler"]["description"] = "Handler ID to close";
    closeTool["inputSchema"]["required"].append("handler");
    tools.append(closeTool);
    
    Json::Value result;
    result["tools"] = tools;
    
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::cout << Json::writeString(writer, response) << std::endl;
}

void sendResourceListChanged() {
    Json::Value notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "notifications/resources/list_changed";
    
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::cout << Json::writeString(writer, notification) << std::endl;
}

void handleCallTool(const Json::Value& id, const Json::Value& params) {
    std::string toolName = params["name"].asString();
    Json::Value arguments = params["arguments"];
    Json::Value result;
    
    try {
        if (toolName == "preload") {
            std::string path = arguments["path"].asString();
            auto segment = registry.preload(path);
            if (segment) {
                std::filesystem::path canonical_path = std::filesystem::canonical(path);
                std::string handler = canonical_path.string();
                
                result["content"][0]["type"] = "text";
                result["content"][0]["text"] = "File preloaded successfully.\n\nHandler: " + handler + 
                                               "\nSize: " + std::to_string(segment->size()) + " bytes" +
                                               "\nResource URI: file:///" + handler;
                
                // Send notification that resources list changed
                Json::Value response = createResponse(id, result);
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                std::cout << Json::writeString(writer, response) << std::endl;
                
                sendResourceListChanged();
                return;
            } else {
                Json::Value response = createError(id, -32000, "Failed to preload file");
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                std::cout << Json::writeString(writer, response) << std::endl;
                return;
            }
        } else if (toolName == "read") {
            std::string handler = arguments["handler"].asString();
            size_t offset = arguments["offset"].asUInt64();
            size_t size = arguments["size"].asUInt64();
            std::string format = arguments.get("format", "text").asString();
            
            auto segment = registry.getByHandler(handler);
            if (!segment) {
                Json::Value response = createError(id, -32000, "Invalid handler: " + handler);
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                std::cout << Json::writeString(writer, response) << std::endl;
                return;
            }
            if (offset + size > segment->size()) {
                Json::Value response = createError(id, -32000, "Read out of bounds. File size: " + 
                                                   std::to_string(segment->size()) + " bytes, requested: " + 
                                                   std::to_string(offset + size) + " bytes");
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                std::cout << Json::writeString(writer, response) << std::endl;
                return;
            }
            
            const char* data = static_cast<const char*>(segment->data()) + offset;
            std::string content;
            
            if (format == "hex") {
                std::stringstream ss;
                for (size_t i = 0; i < size; ++i) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)(unsigned char)data[i];
                }
                content = ss.str();
            } else {
                content = std::string(data, size);
            }
            
            result["content"][0]["type"] = "text";
            result["content"][0]["text"] = content;
        } else if (toolName == "close") {
            std::string handler = arguments["handler"].asString();
            registry.close(handler);
            
            result["content"][0]["type"] = "text";
            result["content"][0]["text"] = "Handler closed successfully: " + handler;
            
            // Send notification that resources list changed
            Json::Value response = createResponse(id, result);
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "";
            std::cout << Json::writeString(writer, response) << std::endl;
            
            sendResourceListChanged();
            return;
        } else {
            Json::Value response = createError(id, -32601, "Unknown tool: " + toolName);
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "";
            std::cout << Json::writeString(writer, response) << std::endl;
            return;
        }
        
        Json::Value response = createResponse(id, result);
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::cout << Json::writeString(writer, response) << std::endl;
    } catch (const std::exception& e) {
        Json::Value response = createError(id, -32000, std::string("Error: ") + e.what());
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::cout << Json::writeString(writer, response) << std::endl;
    }
}

void processRequest(const std::string& line) {
    Json::CharReaderBuilder builder;
    Json::Value request;
    std::string errs;
    
    std::istringstream iss(line);
    if (!Json::parseFromStream(builder, iss, &request, &errs)) {
        std::cerr << "JSON parse error: " << errs << std::endl;
        return;
    }
    
    std::string method = request["method"].asString();
    Json::Value id = request["id"];
    Json::Value params = request["params"];
    
    if (method == "initialize") {
        handleInitialize(id);
    } else if (method == "tools/list") {
        handleListTools(id);
    } else if (method == "tools/call") {
        handleCallTool(id, params);
    } else if (method == "resources/list") {
        handleListResources(id);
    } else if (method == "resources/read") {
        handleReadResource(id, params);
    } else if (method == "notifications/initialized") {
        // No response needed for notifications
    } else {
        Json::Value response = createError(id, -32601, "Method not found: " + method);
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::cout << Json::writeString(writer, response) << std::endl;
    }
}

int main() {
    std::string line;
    std::cerr << "MCP stdio server started" << std::endl;
    
    while (std::getline(std::cin, line)) {
        if (!line.empty()) {
            processRequest(line);
        }
    }
    
    return 0;
}
