// #pragma once

// #include <assert.h>
// #include <atomic>
// #include <deque>
// #include <format>
// #include <functional>
// #include <iostream>
// #include <list>
// #include <mutex>
// #include <ranges>
// #include <thread>

// namespace cq {

// struct ThreadPool {
//     ThreadPool(size_t threadsCount) : terminate(false), id(std::this_thread::get_id()) {
//         workers.reserve(threadsCount);
//         for (auto _ : std::views::iota(size_t(0), threadsCount)) {
//             addThread();
//         }
//     }
//     template <typename F> void post(F &&func) {
//         std::unique_lock lock{mtx};
//         tasks.emplace_back(std::forward<F>(func));
//         cv.notify_all();
//     }
//     void waitForEmpty() {
//         std::unique_lock lock{mtx};
//         assert(id == std::this_thread::get_id());
//         cv_wait.wait(lock, [this] { return this->terminate || this->tasks.empty(); });
//     }
//     bool doTask() {
//         std::function<void()> task;
//         {
//             std::unique_lock lock{mtx};
//             cv.wait(lock, [this] { return this->terminate || (this->tasks.size() != 0); });
//             if (terminate) {
//                 return false;
//             }
//             task = std::move(tasks.front());
//             tasks.pop_front();
//         }
//         cv_wait.notify_all();
//         task();
//         return true;
//     }
//     // void forceTerminate(){
//     //     terminate = true;
//     //     cv.notify_all();
//     //     for(auto& t : workers){
//     //         t.request_stop();
//     //     }
//     // }
//     ~ThreadPool() {
//         std::unique_lock lock{mtx};
//         assert(id == std::this_thread::get_id());
//         terminate = true;
//         cv.notify_all();
//         cv_wait.notify_all();
//     }

//   private:
//     std::thread::id id;
//     bool terminate;
//     std::mutex mtx;
//     std::condition_variable cv;
//     std::condition_variable cv_wait;
//     std::vector<std::jthread> workers;
//     std::deque<std::function<void()>> tasks;
//     void addThread() {
//         workers.emplace_back([this] {
//             while (this->doTask());
//         });
//     }
// };

// } // namespace cq