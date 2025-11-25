#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <functional>

class SSEBroadcaster {
public:
    void subscribe(std::function<void(const std::string&)> sendEvent);
    void unsubscribe(std::function<void(const std::string&)> sendEvent);
    void broadcast(const std::string& type, const std::string& data);
private:
    std::vector<std::function<void(const std::string&)>> clients;
    std::mutex mtx;
};
