#pragma once

#include <chrono>
#include <initializer_list>
#include <map>
#include <string>

struct Profiler {
    Profiler(std::initializer_list<std::string> l) {
#ifdef __ENABLE_PROFILE
        for (auto &name : l) {
            logs[name] = Log{0, std::chrono::nanoseconds::zero()};
        }
#endif
    }
    struct Counter {
#ifdef __ENABLE_PROFILE
        Counter(Profiler *p, std::string_view id) : t1(std::chrono::high_resolution_clock::now()), p(p), id(id) {}
        void end(){
            auto t2 = std::chrono::high_resolution_clock::now() - t1;
            p->log(id, t2);
            p = nullptr;
        }
#else
        Counter(Profiler *p, std::string_view id) {}
        void end(){}
#endif
#ifdef __ENABLE_PROFILE
        ~Counter() {
            if (p) {
                auto t2 = std::chrono::high_resolution_clock::now() - t1;
                p->log(id, t2);
            }
        }
#endif
#ifdef __ENABLE_PROFILE
        Profiler *p;
        std::string_view id;
        std::chrono::steady_clock::time_point t1;
#endif
    };
    Counter startRecord(std::string_view id, Counter* end=nullptr) { 
        if(end){
            end->end();
        }
        return Counter{this, id};
    }
    void log(std::string_view id, std::chrono::nanoseconds time) {
#ifdef __ENABLE_PROFILE
        logs.find(id)->second.times++;
        logs.find(id)->second.totalTime += time;
#endif
    }
#ifdef __ENABLE_PROFILE
    struct Log {
        size_t times;
        std::chrono::nanoseconds totalTime;
    };
    std::map<std::string, Log, std::less<>> logs;
    std::string getResult(){
        std::string ret;
        for(auto&& [name, log] : logs){
            ret += std::format("{}: called {} times, ~ {} ns / time\n", name, log.times, log.totalTime.count() / log.times);
        }
        return ret;
    }
#endif
};