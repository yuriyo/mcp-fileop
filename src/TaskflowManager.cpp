#include "TaskflowManager.hpp"

TaskflowManager::TaskflowManager() : exec() {}

tf::Executor& TaskflowManager::executor() {
    return exec;
}
