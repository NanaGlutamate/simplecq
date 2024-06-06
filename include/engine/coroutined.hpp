#include <coroutine>

#include "taskflow/taskflow.hpp"

struct PublishSubscriberManager {
    struct promise_type;
    struct task : std::coroutine_handle<promise_type> {};
};

struct SimEngine {
    tf::Executor executor = tf::Executor{};
};                                                                                                                                         