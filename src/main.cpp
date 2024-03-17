#include <format>
#include <iostream>

#include "unifex/any_scheduler.hpp"

#include "csmodel_base.h"
#include "thread_pool.hpp"

int main() {
    using namespace cq;
    std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "post") << std::endl;
    {
        std::mutex mtx;
        ThreadPool tp{8};
        for (size_t i = 0; i < 10; i++) {
            tp.post([&] {
                for (auto i = 0; i < 10000; ++i) {
                    {
                        std::unique_lock lock{mtx};
                        std::cout << std::format("[{}]: {}", std::this_thread::get_id(), i) << std::endl;
                    }
                }
            });
        }
        // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "posted and wait") << std::endl;
        tp.waitForEmpty();
        // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "done") << std::endl;
    }
    std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "end") << std::endl;
    return 0;
}