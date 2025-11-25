#pragma once
#include <taskflow/taskflow.hpp>

class TaskflowManager {
public:
    TaskflowManager();
    tf::Executor& executor();
private:
    tf::Executor exec;
};
