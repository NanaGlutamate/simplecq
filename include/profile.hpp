#pragma once

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

struct Profiler {
    struct Counter {
#ifdef __ENABLE_PROFILE
        Counter(Profiler *p, std::string id) : t1(std::chrono::high_resolution_clock::now()), p(p), id(std::move(id)) {}
        void end() {
            auto t2 = std::chrono::high_resolution_clock::now() - t1;
            p->log(id, t2);
            p = nullptr;
        }
#else
        Counter(Profiler *p, const std::string &id) {}
        void end() {}
#endif
#ifdef __ENABLE_PROFILE
        ~Counter() {
            if (p) {
                end();
            }
        }
#endif
#ifdef __ENABLE_PROFILE
        Profiler *p;
        std::string id;
        std::chrono::steady_clock::time_point t1;
#endif
    };
    Counter startRecord(std::string id, Counter *end = nullptr) {
        if (end) {
            end->end();
        }
        return Counter{this, std::move(id)};
    }
    void log(const std::string &id, std::chrono::nanoseconds time) {
#ifdef __ENABLE_PROFILE
        logs[id].times++;
        logs[id].totalTime += time;
#endif
    }
#ifdef __ENABLE_PROFILE
    struct Log {
        size_t times;
        std::chrono::nanoseconds totalTime;
    };
    std::map<std::string, Log, std::less<>> logs;
    std::string getResult() {
        std::vector<std::tuple<size_t, std::string>> ans;
        size_t max_size = 0;
        for (auto &&[name, log] : logs) {
            max_size = max(max_size, name.size());
        }
        ans.reserve(logs.size());
        for (auto &&[name, log] : logs) {
            size_t t = log.totalTime.count() / 1000;
            double rate = 1.;
            std::string s = "us";
            if (t > 1e6){
                rate = 1000000;
                s = " s";
            }else if(t > 1e3){
                rate = 1000;
                s = "ms";
            }
            ans.emplace_back(t, std::format("[{:>{}}]: {:10.4} {} / {:10} times = {:10.3}\n", name, max_size + 1,
                                            double(t) / rate, s, log.times, double(t) / double(log.times)));
        }
        std::sort(ans.begin(), ans.end(), [](const auto &l, const auto &r) { return std::get<0>(l) > std::get<0>(r); });
        std::string ret;
        for (auto &&[_, line] : ans) {
            ret += std::move(line);
        }
        return ret;
    }
#endif
};