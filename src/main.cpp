#include <format>
#include <iostream>
#include <string>

#include <taskflow/taskflow.hpp>

#include "csmodel_base.h"
#include "mysock.hpp"
#include "thread_pool.hpp"
#include "taskgraph.hpp"

int main() {
    using namespace std;
    tf::Executor executor;
    tf::Taskflow taskflow;
    auto A = taskflow.emplace([] { return 1; });
    auto B = taskflow.emplace([]() { return 1; });
    auto C = taskflow.emplace([]() { std::cout << 1; });
    A.precede(B); // A runs before B and C
    C.succeed(B); // D runs after  B and C
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