/**
 * @file executionengine.hpp
 * @author glutamate
 * @brief
 * @version 0.1
 * @date 2024-05-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <any>
#include <array>
#include <expected>
#include <ranges>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "datatransform.hpp"
#include "dowithcatch.hpp"
#include "engine/modelmanager.hpp"
#include "taskflow/taskflow.hpp"

using CSValueMap = std::unordered_map<std::string, std::any>;

/**
 * @brief padding type to avoid false sharing
 *
 * @tparam Ty original type
 */
template <typename Ty>
    requires std::is_class_v<Ty>
struct alignas(128) CacheLinePadding : public Ty {};

struct TopicManager {
    TopicManager() = default;
    TopicManager(const TopicManager &) = delete;

    void clear() {
        topics.clear();
        dependenciesOfTarget.clear();
        // TODO: UB?
        buffer.~TopicBuffer();
        new (&buffer) TopicBuffer{};
    }

    struct TopicInfo {
        std::vector<std::string> members;
        TransformInfo trans;
        bool canAssembleFrom(const CSValueMap &data) const {
            for (auto &&name : members) {
                if (!data.contains(name)) {
                    return false;
                }
            }
            return true;
        }
        std::set<std::string> getTargets() const {
            std::set<std::string> ret;
            for (auto &&[k, v] : trans.rules) {
                for (auto &&[_, acts] : v) {
                    for (auto &&act : acts) {
                        ret.emplace(act.to);
                    }
                }
            }
            return ret;
        }
    };

    // src type -> sent topics
    using ModelTopics = std::unordered_map<std::string, std::vector<TopicInfo>>;
    ModelTopics topics;

    using ClassifiedModelOutput = std::unordered_map<std::string, std::vector<CSValueMap>>;

    struct TopicBuffer {
        TopicBuffer() {
            topic_buffer = &buffer0;
            preparing_topic_buffer = &buffer1;
        };
        TopicBuffer(const TopicBuffer &) = delete;
        void operator=(const TopicBuffer &) = delete;
        // model_id(src) -> model_type_name(target) -> topics
        std::vector<CacheLinePadding<ClassifiedModelOutput>> output_buffer = {};
        std::vector<CacheLinePadding<ClassifiedModelOutput>> dyn_output_buffer = {};
        // model_type_name -> topics received by that model
        CacheLinePadding<ClassifiedModelOutput> buffer0, buffer1;
        CacheLinePadding<ClassifiedModelOutput> *topic_buffer = &buffer0;
        CacheLinePadding<ClassifiedModelOutput> *preparing_topic_buffer = &buffer1;
        // void clearTopicBuffer() {
        //     for (auto &&[k, v] : topic_buffer) {
        //         v.clear();
        //     }
        // }
        void swapBuffer() { std::swap(topic_buffer, preparing_topic_buffer); }
    } buffer;

    std::unordered_map<std::string, std::vector<size_t>> dependenciesOfTarget;
    // void topicCollect(tf::Subflow &sbf) {
    //     std::unordered_map<std::string_view, tf::Task> dependencies;
    //     for (auto &&[target, topics] : *buffer.topic_buffer) {
    //         dependencies[target] = sbf.emplace([&] { topics.clear(); });
    //     }
    //     auto doTransform = [&, this](std::vector<CacheLinePadding<ClassifiedModelOutput>> &tar) {
    //         for (auto &&output : tar) {
    //             for (auto &&[k, v] : output) {
    //                 if (!buffer.topic_buffer->contains(k)) {
    //                     (*buffer.topic_buffer)[k];
    //                 }
    //                 auto it = dependencies.find(k);
    //                 auto functor = [&, &buffer{buffer}] {
    //                     auto &target_buffer = buffer.topic_buffer->find(k)->second;
    //                     target_buffer.insert_range(target_buffer.end(), std::move(v));
    //                     v.clear();
    //                 };
    //                 tf::Task t = sbf.emplace(std::move(functor));
    //                 if (it != dependencies.end()) {
    //                     t.succeed(it->second);
    //                     it->second = t;
    //                 } else {
    //                     dependencies.emplace(k, t);
    //                 }
    //             }
    //         }
    //     };
    //     doTransform(buffer.output_buffer);
    //     doTransform(buffer.dyn_output_buffer);
    //     sbf.join();
    // }
    void dynamicTopicCollect(tf::Subflow &sbf) {
        std::unordered_map<std::string_view, tf::Task> dependencies;
        for (auto &&[target, topics] : *buffer.preparing_topic_buffer) {
            dependencies[target] = sbf.emplace([&] { topics.clear(); });
        }
        for (auto &&output : buffer.dyn_output_buffer) {
            for (auto &&[k, v] : output) {
                // TODO: remove?
                if (!buffer.preparing_topic_buffer->contains(k)) {
                    (*buffer.preparing_topic_buffer)[k];
                }
                auto it = dependencies.find(k);
                tf::Task t = sbf.emplace([&, &buffer{buffer}] {
                    auto &target_buffer = buffer.preparing_topic_buffer->find(k)->second;
                    target_buffer.insert_range(target_buffer.end(), std::move(v));
                    v.clear();
                });
                if (it != dependencies.end()) {
                    t.succeed(it->second);
                    it->second = t;
                } else {
                    dependencies.emplace(k, t);
                }
            }
        }
    }
    void staticTopicCollect(const std::string &target) {
        auto &target_buffer = buffer.preparing_topic_buffer->find(target)->second;
        auto it = dependenciesOfTarget.find(target);
        if (it == dependenciesOfTarget.end()) [[unlikely]] {
            return;
        }
        for (size_t modelID : it->second) {
            auto &output = buffer.output_buffer[modelID];
            auto it = output.find(target);
            if (it == output.end()) {
                continue;
            }
            target_buffer.insert_range(target_buffer.end(), std::move(it->second));
            it->second.clear();
        }
    }
};

struct ExecutionEngine {
    TopicManager tm = {};
    ModelManager mm = {};
    struct State {
        double dt = 100; //< deltatime, ms
        size_t loop = 0;
        double fps = 0.;
    } s;

    tf::Executor executor = tf::Executor{};
    tf::Taskflow frame = {};

    void clear() {
        frame.clear();
        mm = {};
        tm.clear();
        s = State{};
    };

    struct ModelOutputFunc {
        ExecutionEngine &self;
        CSModelObject *obj;
        std::string model_type;
        bool movable;
        TopicManager::ClassifiedModelOutput &ret;
        std::vector<TopicManager::TopicInfo> *topic_list;
        bool no_output_topic;
        void operator()() {
            CSValueMap *model_output_ptr = nullptr;
            doWithCatch([&, obj{obj}] {
                model_output_ptr = obj->GetOutput();
            }).or_else([&, this](const std::string &err) -> std::expected<void, std::string> {
                self.mm.callback.writeLog("Engine", std::format("Exception When Model[{}] Output: {}", model_type, err),
                                          5);
                return {};
            });
            if (no_output_topic || !model_output_ptr) {
                return;
            }
            for (auto &&[n, v] : ret) {
                // TODO:
                v.clear();
            }
            for (auto &&[cnt, topic] : std::views::enumerate(*topic_list)) {
                if (!topic.canAssembleFrom(*model_output_ptr)) {
                    continue;
                }
                std::array<TransformInfo::InputBuffer, 1> buffer{
                    {model_type, model_output_ptr, (cnt == topic_list->size() - 1) ? movable : false}};
                // marks.data.clear();
                topic.trans.transformWithCallback(
                    [&, marks{std::set<std::string_view>{}}]<typename Ty>(const std::string &a, const std::string &b,
                                                                          Ty &&c) mutable {
                        if (!marks.contains(a)) {
                            marks.emplace(a);
                            ret[a].emplace_back();
                        }
                        ret.find(a)->second.back().emplace(b, std::forward<Ty>(c));
                    },
                    std::span{buffer});
            }
        }
    };

    struct ModelInputFunc {
        ExecutionEngine &self;
        CSModelObject *obj;
        std::string model_type;
        void operator()() {
            if (auto it = self.tm.buffer.topic_buffer->find(model_type); it != self.tm.buffer.topic_buffer->end()) {
                for (auto &&v : it->second) {
                    doWithCatch([&] {
                        obj->SetInput(v);
                    }).or_else([&, this](const std::string &err) -> std::expected<void, std::string> {
                        self.mm.callback.writeLog("Engine",
                                                  std::format("Exception When Model[{}] Input: {}\n{}", model_type, err,
                                                              tools::myany::printCSValueMapToString(v)),
                                                  5);
                        return {};
                    });
                }
            }
        }
    };

    struct ModelTickFunc {
        ExecutionEngine &self;
        CSModelObject *obj;
        std::string model_type;
        void operator()() {
            doWithCatch([&] {
                obj->Tick(self.s.dt);
            }).or_else([this](const std::string &err) -> std::expected<void, std::string> {
                self.mm.callback.writeLog("Engine", std::format("Exception When Model[{}] Tick: {}", model_type, err),
                                          5);
                return {};
            });
        }
    };

    void buildGraph() {
        std::map<std::string, std::set<std::string>> targets;
        for (auto &&[model_type, topics] : tm.topics) {
            for (auto &&topic : topics) {
                targets[model_type].merge(topic.getTargets());
            }
        }

        auto collect_task = frame.emplace([this](tf::Subflow &sbf) {
            // tm.topicCollect(sbf);
            tm.buffer.swapBuffer();
            s.loop--;
        });
        collect_task.name("collect output");

        auto dyn_collector = frame.emplace([this](tf::Subflow &sbf) { tm.dynamicTopicCollect(sbf); });
        dyn_collector.precede(collect_task).name("collect topic for dyn model");
        std::unordered_map<std::string, tf::Task> collector;
        for (auto &&type : targets | std::views::values | std::views::join) {
            if (collector.contains(type)) {
                continue;
            }
            tf::Task sub_collect_task = frame.emplace([this, type] { tm.staticTopicCollect(type); });
            sub_collect_task.succeed(dyn_collector).precede(collect_task).name("collect topic for " + type);

            tm.buffer.topic_buffer->emplace(type, TopicManager::ClassifiedModelOutput::mapped_type{});
            tm.buffer.preparing_topic_buffer->emplace(type, TopicManager::ClassifiedModelOutput::mapped_type{});
            collector.emplace(type, sub_collect_task);
        }

        std::unordered_map<std::string, std::vector<tf::Task>> output_tasks;

        // dynamic tasks
        auto dyn_output_task = frame.emplace([this](tf::Subflow &sbf) {
            // create model
            mm.createDynamicModel();
            // output
            tm.buffer.dyn_output_buffer.resize(mm.dynamicModels.size());
            for (auto &&[idx, data] : std::views::enumerate(mm.dynamicModels)) {
                auto it = tm.topics.find(data.modelTypeName);
                bool no_output = (it == tm.topics.end());
                sbf.emplace(ModelOutputFunc{*this, data.handle.obj, data.modelTypeName, data.handle.outputDataMovable,
                                            tm.buffer.dyn_output_buffer[idx], no_output ? nullptr : &it->second,
                                            no_output});
            }
            sbf.join();
        });
        dyn_output_task.name(std::format("dynamic::output")).precede(dyn_collector);

        auto dyn_init_task = frame.emplace([] { return 0; }).name("dynamic::start loop").precede(dyn_output_task);

        auto dyn_input_task = frame.emplace([this](tf::Subflow &sbf) {
            for (auto &&[model_type, handle] : mm.dynamicModels) {
                sbf.emplace(ModelInputFunc{*this, handle.obj, model_type});
            }
            sbf.join();
        });
        dyn_input_task.name("dynamic::input").succeed(collect_task);

        auto dyn_tick_task = frame.emplace([this](tf::Subflow &sbf) {
            for (auto &&[model_type, handle] : mm.dynamicModels) {
                sbf.emplace(ModelTickFunc{*this, handle.obj, model_type});
            }
            sbf.join();
        });
        dyn_tick_task.name("dynamic::tick").succeed(dyn_input_task);

        auto dyn_loop_condition = frame.emplace([this] { return s.loop != 0 ? 0 : 1; });
        dyn_loop_condition.name("dynamic::next frame condition").precede(dyn_output_task).succeed(dyn_tick_task);

        // static tasks
        tm.buffer.output_buffer.resize(mm.models.size());

        for (auto &&[model_id, model_entity] : std::views::enumerate(mm.models)) {
            auto &&[model_type, model_info] = model_entity;
            auto it = tm.topics.find(model_type);

            bool no_output = (it == tm.topics.end());
            auto output_task = frame.emplace(
                ModelOutputFunc{*this, model_info.obj, model_type, model_info.outputDataMovable,
                                tm.buffer.output_buffer[model_id], no_output ? nullptr : &it->second, no_output});
            output_task.name(std::format("{}[{}]::output", model_type, model_info.obj->GetID()));

            // find dependencies
            for (auto &&target : targets[model_type]) {
                tm.dependenciesOfTarget[target].emplace_back(model_id);
                if (auto it = collector.find(target); it != collector.end()) {
                    output_task.precede(it->second);
                }
            }

            auto init_task = frame.emplace([] { return 0; });
            init_task.name(std::format("{}[{}]::start loop", model_type, model_info.obj->GetID())).precede(output_task);

            auto input_task = frame.emplace(ModelInputFunc{*this, model_info.obj, model_type});
            input_task.name(std::format("{}[{}]::input", model_type, model_info.obj->GetID())).succeed(collect_task);

            auto tick_task = frame.emplace(ModelTickFunc{*this, model_info.obj, model_type});
            tick_task.name(std::format("{}[{}]::tick", model_type, model_info.obj->GetID())).succeed(input_task);

            auto loop_condition = frame.emplace([this] { return s.loop != 0 ? 0 : 1; });
            loop_condition.name(std::format("{}[{}]::next frame condition", model_type, model_info.obj->GetID()))
                .precede(output_task)
                .succeed(tick_task);
        }
    }

    /**
     * @brief call engine to run n times
     *
     * @param times times to run
     */
    void run(size_t times = 1) {
        using namespace std::chrono;
        if (times == 0) {
            return;
        }
        s.loop = times;
        auto p = high_resolution_clock::now();
        executor.run(frame).wait();
        auto t = high_resolution_clock::now() - p;
        double t2 = double(duration_cast<microseconds>(t).count());
        s.fps = double(times) / (t2 * microseconds::period::num / microseconds::period::den);
    }
};