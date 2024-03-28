#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "stringprocess.hpp"

struct Profiler {
    struct Counter {
#ifdef __ENABLE_PROFILE
        Counter(Profiler *p, std::string id) : t1(std::chrono::high_resolution_clock::now()), p(p), id(std::move(id)) {}
        void end() {
            auto t2 = std::chrono::high_resolution_clock::now() - t1;
            p->log(id, t2);
            p = nullptr;
        }
        ~Counter() {
            if (p) {
                end();
            }
        }
        Profiler *p;
        std::string id;
        std::chrono::steady_clock::time_point t1;
#else
        void end() {}
#endif
    };
    Counter startRecord(std::string id, Counter *end = nullptr) {
#ifdef __ENABLE_PROFILE
        if (end) {
            end->end();
        }
        return Counter{this, std::move(id)};
#else
        return Counter{};
#endif
    }
#ifdef __ENABLE_PROFILE
    void log(const std::string &id, std::chrono::nanoseconds time) {
        logs[id].times++;
        logs[id].totalTime += time;
    }
    struct Log {
        size_t times;
        std::chrono::nanoseconds totalTime;
    };
    std::map<std::string, Log, std::less<>> logs;
    std::string getResult() {
        using std::to_string;
        std::vector<std::array<std::string, 9>> ans;
        ans.reserve(logs.size());
        for (auto &&[name, log] : logs) {
            auto t = std::chrono::duration_cast<std::chrono::microseconds>(log.totalTime);
            ans.push_back({"[", std::move(name), " ]: ", to_string(t.count()), " us / ", to_string(log.times),
                           " times = ", to_string(double(t.count()) / log.times), " us\n"});
        }
        std::sort(ans.begin(), ans.end(),
                  [](const auto &l, const auto &r) { return std::stoi(l[3]) > std::stoi(r[3]); });
        constexpr std::array<bool, 9> alignConfig{true, false, true, true, true, true, true, true, true};
        tools::mystr::align(ans, alignConfig);
        return tools::mystr::join(
            ans | std::views::transform([](auto &v) { return tools::mystr::join(std::move(v), ""); }), "");
    }
#else
    std::string getResult() { return "[profile disabled]"; }
#endif
};