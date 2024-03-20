#include <format>
#include <iostream>
#include <mutex>
#include <memory>

#include <taskflow/taskflow.hpp>
#include <yaml-cpp/yaml.h>

#include "csmodel_base.h"
#include "dllop.hpp"
#include "thread_pool.hpp"

int main() {
    using namespace cq;

    tf::Executor executor;
    tf::Taskflow taskflow;

    auto [A, B, C, D] = taskflow.emplace( // create four tasks
        [] { std::cout << "TaskA\n"
                       << std::endl; }, [] { std::cout << "TaskB\n"
                                                                    << std::endl; },
        [] { std::cout << "TaskC\n"
                       << std::endl; }, [] { std::cout << "TaskD\n"
                                                                    << std::endl; });

    A.precede(B, C); // A runs before B and C
    D.succeed(B, C); // D runs after  B and C

    executor.run(taskflow).wait();
    // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "post") << std::endl;
    // {
    //     std::mutex mtx;
    //     ThreadPool tp{8};
    //     for (size_t i = 0; i < 10; i++) {
    //         tp.post([&] {
    //             for (auto i = 0; i < 10000; ++i) {
    //                 {
    //                     std::unique_lock lock{mtx};
    //                     std::cout << std::format("[{}]: {}", std::this_thread::get_id(), i) << std::endl;
    //                 }
    //             }
    //         });
    //     }
    //     // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "posted and wait") << std::endl;
    //     tp.waitForEmpty();
    //     // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "done") << std::endl;
    // }
    // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "end") << std::endl;
    return 0;
}

struct TinyCQ {
    tf::Executor executor;
    tf::Taskflow taskflow;
};

namespace {

std::unique_ptr<TinyCQ> cqInstance;

} // namespace

extern "C" {
__declspec(dllexport) bool CreateCQInstance() {
    if (cqInstance){
        return false;
    }
    cqInstance = std::make_unique<TinyCQ>();
    return true;
}
__declspec(dllexport) bool SendMessage(int id, const char *msg, size_t msgLen, const char *topicName,
                                       size_t topicNameLen) {
    return true;
};
}