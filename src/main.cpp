#include <drogon/drogon.h>
#include <variant>
#include <optional>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include "SegmentRegistry.hpp"
#include "TaskflowManager.hpp"

SegmentRegistry registry;
TaskflowManager taskflow;

void handleMcpRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
    auto json = req->getJsonObject();
    if (!json) {
        Json::Value resp(Json::objectValue);
        resp["error"]["code"] = "invalid_request";
        resp["error"]["message"] = "Invalid JSON";
        auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
        httpResp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        cb(httpResp);
        return;
    }

    std::string op = (*json)["op"].asString();
    Json::Value response(Json::objectValue);

    if (op == "preload") {
        std::string path = (*json)["params"]["path"].asString();
        try {
            auto segment = registry.preload(path);
            if (segment) {
                // Use canonical path as handler
                std::filesystem::path canonical_path = std::filesystem::canonical(path);
                response["handler"] = canonical_path.string();
                // broadcaster.broadcast("preload_success", path);
            } else {
                response["error"]["code"] = "preload_failed";
                response["error"]["message"] = "Failed to preload file";
            }
        } catch (const std::exception& e) {
            response["error"]["code"] = "preload_error";
            response["error"]["message"] = e.what();
        }
    } else if (op == "read") {
        std::string handler = (*json)["params"]["handler"].asString();
        size_t offset = (*json)["params"]["offset"].asUInt64();
        size_t size = (*json)["params"]["size"].asUInt64();
        std::string format = (*json)["params"]["format"].asString();
        try {
            auto segment = registry.getByHandler(handler);
            if (!segment) {
                response["error"]["code"] = "read_failed";
                response["error"]["message"] = "Invalid handler";
            } else if (offset + size > segment->size()) {
                response["error"]["code"] = "read_failed";
                response["error"]["message"] = "Read out of bounds";
            } else {
                const char* data = static_cast<const char*>(segment->data()) + offset;
                if (format == "binary") {
                    response["data"] = std::string(data, size);
                } else if (format == "hex") {
                    std::stringstream ss;
                    for (size_t i = 0; i < size; ++i) {
                        ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)(unsigned char)data[i];
                    }
                    response["data"] = ss.str();
                } else if (format == "text") {
                    response["data"] = std::string(data, size);
                } else {
                    response["error"]["code"] = "read_failed";
                    response["error"]["message"] = "Invalid format";
                }
            }
        } catch (const std::exception& e) {
            response["error"]["code"] = "read_error";
            response["error"]["message"] = e.what();
        }
    } else if (op == "close") {
        std::string handler = (*json)["params"]["handler"].asString();
        try {
            registry.close(handler);
            // broadcaster.broadcast("close_success", handler);
        } catch (const std::exception& e) {
            response["error"]["code"] = "close_error";
            response["error"]["message"] = e.what();
        }
    } else {
        response["error"]["code"] = "invalid_operation";
        response["error"]["message"] = "Unknown operation";
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    if (response.isMember("error")) {
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
    }
    cb(resp);
}

void handleEvents(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->addHeader("Content-Type", "text/event-stream");
    resp->addHeader("Cache-Control", "no-cache");
    resp->addHeader("Connection", "keep-alive");
    resp->setBody("data: connected\n\n");
    cb(resp);
}

int main() {
    using namespace drogon;
    app().registerHandler("/mcp", [](const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) {
        handleMcpRequest(req, std::move(callback));
    }, {Post});
    app().registerHandler("/events", [](const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) {
        handleEvents(req, std::move(callback));
    }, {Get});
    app().loadConfigFile("config.json"); // Optional config
    std::cout << "Server starting on port 8080" << std::endl;
    app().run();
    return 0;
}
