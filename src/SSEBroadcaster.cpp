#include "SSEBroadcaster.hpp"
#include <algorithm>

void SSEBroadcaster::subscribe(std::function<void(const std::string&)> sendEvent) {
    std::lock_guard lock(mtx);
    clients.push_back(sendEvent);
}

void SSEBroadcaster::unsubscribe(std::function<void(const std::string&)> sendEvent) {
    // Since std::function can't be compared, we can't remove specific clients
    // For now, do nothing. In a real implementation, use a different mechanism.
}

void SSEBroadcaster::broadcast(const std::string& type, const std::string& data) {
    std::lock_guard lock(mtx);
    std::string event = "event: " + type + "\ndata: " + data + "\n\n";
    for (auto& client : clients) {
        client(event);
    }
}
