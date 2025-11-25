#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include "MemorySegment.hpp"

class SegmentRegistry {
public:
    std::shared_ptr<MemorySegment> preload(const std::string& path);
    std::shared_ptr<MemorySegment> getByHandler(const std::string& handler);
    void close(const std::string& handler);
    std::vector<std::string> listHandlers() const;
    void setAllowedPaths(const std::vector<std::string>& paths);
    bool isPathAllowed(const std::string& path) const;

private:
    std::unordered_map<std::string, std::shared_ptr<MemorySegment>> pathMap;
    std::unordered_map<std::string, std::weak_ptr<MemorySegment>> handlerMap;
    std::vector<std::string> allowedPaths;
    mutable std::shared_mutex mutex;
};
