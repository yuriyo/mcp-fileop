#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <json/json.h>
#include "FileOpController.hpp"
// SegmentRegistry is now managed via FileOpController

FileOpController controller;

// Keep local helper wrappers for backward-compat with existing code
inline Json::Value createResponse(const Json::Value& id, const Json::Value& result) {
    return controller.createResponse(id, result);
}

inline Json::Value createError(const Json::Value& id, int code, const std::string& message) {
    return controller.createError(id, code, message);
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
    Json::Value result = controller.listResources();
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";  // Compact output
    std::cout << Json::writeString(writer, response) << std::endl;
}

void handleReadResource(const Json::Value& id, const Json::Value& params) {
    Json::Value result = controller.readResourceFromUri(params);
    if (result.isMember("__error__")) {
        Json::Value response = createError(id, -32000, result["__error__"].asString());
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::cout << Json::writeString(writer, response) << std::endl;
        return;
    }
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::cout << Json::writeString(writer, response) << std::endl;
}

void handleListTools(const Json::Value& id) {
    Json::Value result = controller.listTools();
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
    auto progressCallback = [](const Json::Value&) {
        // stdio doesn't emit progress updates
    };
    Json::Value result = controller.callTool(params, progressCallback);
    if (result.isMember("__error__")) {
        Json::Value response = createError(id, -32000, result["__error__"].asString());
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::cout << Json::writeString(writer, response) << std::endl;
        return;
    }
    Json::Value response = createResponse(id, result);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::cout << Json::writeString(writer, response) << std::endl;
    if (result.get("resourceListChanged", false).asBool()) {
        sendResourceListChanged();
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
