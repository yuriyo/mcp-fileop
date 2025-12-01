#pragma once
#include <json/json.h>
#include <functional>
#include <string>
#include "SegmentRegistry.hpp"

class FileOpController {
public:
    explicit FileOpController();

    Json::Value createResponse(const Json::Value& id, const Json::Value& result) const;
    Json::Value createError(const Json::Value& id, int code, const std::string& message) const;

    // Resources and tools
    Json::Value listTools() const;
    Json::Value listResources();
    Json::Value readResourceFromUri(const Json::Value& params);

    // Call tool by name. Optional progress callback invoked with progress updates during read_multiple.
    // Returns a Json::Value suitable as the 'result' field for a JSON-RPC response.
    Json::Value callTool(const Json::Value& params, std::function<void(const Json::Value&)> progress = nullptr);

    // Configure allowed paths
    void setAllowedPaths(const std::vector<std::string>& paths);

private:
    SegmentRegistry registry_;
};
