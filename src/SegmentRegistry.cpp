#include "SegmentRegistry.hpp"
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <mutex>

void SegmentRegistry::setAllowedPaths(const std::vector<std::string>& paths) {
    std::unique_lock lock(mutex);
    allowedPaths.clear();
    for (const auto& path : paths) {
        try {
            allowedPaths.push_back(std::filesystem::canonical(path).string());
        } catch (const std::filesystem::filesystem_error&) {
            // Skip invalid paths
        }
    }
}

bool SegmentRegistry::isPathAllowed(const std::string& path) const {
    if (allowedPaths.empty()) {
        return true; // If no restrictions configured, allow all
    }
    
    std::string canonical;
    try {
        canonical = std::filesystem::canonical(path).string();
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    }
    
    // Check if path is under any allowed path
    for (const auto& allowed : allowedPaths) {
        if (canonical == allowed || 
            canonical.substr(0, allowed.length() + 1) == allowed + "/") {
            return true;
        }
    }
    return false;
}

std::shared_ptr<MemorySegment> SegmentRegistry::preload(const std::string& path) {
    std::unique_lock lock(mutex);
    
    // Check if path is allowed
    if (!isPathAllowed(path)) {
        throw std::runtime_error("Access denied: path not in allowed list");
    }
    
    auto canonical = std::filesystem::canonical(path).string();
    auto it = pathMap.find(canonical);
    if (it != pathMap.end()) {
        it->second->incRef();
        return it->second;
    }
    auto segment = std::make_shared<MemorySegment>(canonical);
    pathMap[canonical] = segment;
    // Generate handler (for demo, use path)
    handlerMap[canonical] = segment;
    return segment;
}

std::shared_ptr<MemorySegment> SegmentRegistry::getByHandler(const std::string& handler) {
    std::shared_lock lock(mutex);
    auto it = handlerMap.find(handler);
    if (it != handlerMap.end()) {
        return it->second.lock();
    }
    return nullptr;
}

void SegmentRegistry::close(const std::string& handler) {
    std::unique_lock lock(mutex);
    auto it = handlerMap.find(handler);
    if (it != handlerMap.end()) {
        auto segment = it->second.lock();
        if (segment) {
            segment->decRef();
            if (segment->refCount() <= 0) {
                // Remove from registry
                for (auto p = pathMap.begin(); p != pathMap.end(); ++p) {
                    if (p->second == segment) {
                        pathMap.erase(p);
                        break;
                    }
                }
                handlerMap.erase(it);
            }
        }
    }
}

std::vector<std::string> SegmentRegistry::listHandlers() const {
    std::shared_lock lock(mutex);
    std::vector<std::string> handlers;
    handlers.reserve(handlerMap.size());
    for (const auto& [handler, weak_seg] : handlerMap) {
        if (!weak_seg.expired()) {
            handlers.push_back(handler);
        }
    }
    return handlers;
}
