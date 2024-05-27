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
        // std::set<std::string> getTargets() const {
        //     std::set<std::string> ret;
        //     for (auto &&[k, v] : trans.rules) {
        //         for (auto &&[_, acts] : v) {
        //             for (auto &&act : acts) {
        //                 ret.emplace(act.to);
        //             }
        //         }
        //     }
        //     return ret;
        // }
    };

    // src type -> sent topics
    using ModelTopics = std::unordered_map<std::string, std::vector<TopicInfo>>;
    ModelTopics topics;

    using ClassifiedModelOutput = std::unordered_map<std::string, std::vector<CSValueMap>>;

    struct TopicBuffer {
        // model_id(src) -> model_type_name(target) -> topics
        std::vector<CacheLinePadding<ClassifiedModelOutput>> output_buffer;
        std::vector<CacheLinePadding<ClassifiedModelOutput>> dyn_output_buffer;
        // model_type_name -> topics received by that model
        ClassifiedModelOutput topic_buffer;
        // void clearTopicBuffer() {
        //     for (auto &&[k, v] : topic_buffer) {
        //         v.clear();
        //     }
        // }
    } buffer;

    // std::unordered_map<std::string, std::vector<size_t>> dependenciesOfTarget;
    // TODO: remove collect, deriectly input through output_buffer
    void topicCollect(tf::Subflow &sbf) {
        std::unordered_map<std::string_view, tf::Task> dependencies;
        for (auto &&[target, topics] : buffer.topic_buffer) {
            dependencies[target] = sbf.emplace([&] { topics.clear(); });
        }
        auto doTransform = [&, this](std::vector<CacheLinePadding<ClassifiedModelOutput>> &tar) {
            for (auto &&output : tar) {
                for (auto &&[k, v] : output) {
                    if (!buffer.topic_buffer.contains(k)) {
                        buffer.topic_buffer[k];
                    }
                    auto it = dependencies.find(k);
                    auto functor = [&, &buffer{buffer}] {
                        auto &target_buffer = buffer.topic_buffer.find(k)->second;
                        target_buffer.insert_range(target_buffer.end(), std::move(v));
                        v.clear();
                    };
                    tf::Task t = sbf.emplace(std::move(functor));
                    if (it != dependencies.end()) {
                        t.succeed(it->second);
                        it->second = t;
                    } else {
                        dependencies.emplace(k, t);
                    }
                }
            }
        };
        doTransform(buffer.output_buffer);
        doTransform(buffer.dyn_output_buffer);
        sbf.join();
    }
    // void dynamicTopicCollect(tf::Subflow &sbf) {
    //     std::unordered_map<std::string_view, tf::Task> dependencies;
    //     for (auto &&output : buffer.dyn_output_buffer) {
    //         for (auto &&[k, v] : output) {
    //             tf::Task t = sbf.emplace([&, this]() {
    //                 auto it = buffer.topic_buffer.find(k);
    //                 if (it == buffer.topic_buffer.end()) {
    //                     // TODO: warning
    //                     return;
    //                 }
    //                 auto &target_buffer = it->second;
    //                 target_buffer.insert_range(target_buffer.end(), std::move(v));
    //                 v.clear();
    //             });
    //             auto it = dependencies.find(k);
    //             if (it == dependencies.end()) {
    //                 dependencies.emplace(k, t);
    //             } else {
    //                 t.succeed(it->second);
    //                 it->second = t;
    //             }
    //         }
    //     }
    // }
    // void staticTopicCollect(const std::string &target) {
    //     auto &target_buffer = buffer.topic_buffer.find(target)->second;
    //     auto it = dependenciesOfTarget.find(target);
    //     if (it == dependenciesOfTarget.end()) {
    //         return;
    //     }
    //     for (size_t modelID : it->second) {
    //         auto &output = buffer.output_buffer[modelID];
    //         auto it = output.find(target);
    //         if (it == output.end()) {
    //             continue;
    //         }
    //         target_buffer.insert_range(target_buffer.end(), std::move(it->second));
    //         it->second.clear();
    //     }
    // }
};

struct ExecutionEngine {
    tf::Executor executor;
    tf::Taskflow frame;

    ModelManager mm = {};
    TopicManager tm = {};
    struct State {
        double dt = 100; //< deltatime, ms
        size_t loop = 0;
        double fps = 0.;
    } s;

    void clear() {
        frame.clear();
        mm = {};
        tm = {};
        s = State{};
    };

    struct ModelOutputFunc {
        ExecutionEngine &self;
        CSModelObject *obj;
        std::string model_type;
        bool movable;
        TopicManager::ClassifiedModelOutput &ret;
        TopicManager::ModelTopics::const_iterator it;
        bool end;
        void operator()() {
            CSValueMap *model_output_ptr;
            doWithCatch([&] {
                model_output_ptr = obj->GetOutput();
            }).or_else([&, this](const std::string &err) -> std::expected<void, std::string> {
                self.mm.callback.writeLog("Engine", std::format("Exception When Model[{}] Output: {}", model_type, err),
                                          5);
                return {};
            });
            if (end) {
                return;
            }
            for (auto &&[n, v] : ret) {
                // TODO:
                v.clear();
            }
            for (auto &&topic : it->second) {
                if (!topic.canAssembleFrom(*model_output_ptr)) {
                    continue;
                }
                std::array<TransformInfo::InputBuffer, 1> buffer{{model_type, model_output_ptr, movable}};
                for (auto &&[n, v] : topic.trans.transform(std::span{buffer})) {
                    ret[n].push_back(std::move(v));
                }
            }
        }
    };
    struct ModelInputFunc {
        ExecutionEngine &self;
        CSModelObject *obj;
        std::string model_type;
        void operator()() {
            if (auto it = self.tm.buffer.topic_buffer.find(model_type); it != self.tm.buffer.topic_buffer.end()) {
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
        // TODO: remove collect
        auto collect_task = frame
                                .emplace([this](tf::Subflow &sbf) {
                                    tm.topicCollect(sbf);
                                    s.loop--;
                                })
                                .name("collect output");

        // auto dyn_collector = frame
        //                          .emplace([this]() {
        //                              tm.buffer.clearTopicBuffer();
        //                              tm.dynamicTopicCollect(sbf);
        //                          })
        //                          .name("collect topic for dyn model");
        // std::unordered_map<std::string, tf::Task> collector;
        // for (auto &&type : mm.modelTypes) {
        //     tm.buffer.topic_buffer[type] = {};
        //     auto singleTopicCollector = [this, type] {
        //         tm.staticTopicCollect(type);
        //     };
        //     collector[type] = frame.emplace(singleTopicCollector)
        //                           .succeed(dyn_collector)
        //                           .precede(collect_task)
        //                           .name("collect topic for " + type);
        // }

        std::unordered_map<std::string, std::vector<tf::Task>> output_tasks;

        // dynamic tasks
        auto dyn_output_func = [this](tf::Subflow &sbf) {
            // create model
            mm.createDynamicModel();
            // output
            tm.buffer.dyn_output_buffer.resize(mm.dynamicModels.size());
            for (auto &&[idx, data] : std::views::enumerate(mm.dynamicModels)) {
                auto it = tm.topics.find(data.modelTypeName);
                sbf.emplace(ModelOutputFunc{*this, data.handle.obj, data.modelTypeName, data.handle.outputDataMovable,
                                            tm.buffer.dyn_output_buffer[idx], it, it == tm.topics.end()});
            }
            sbf.join();
        };
        auto dyn_output_task =
            frame.emplace(dyn_output_func).name(std::format("dynamic::output")).precede(collect_task);

        auto dyn_init_task = frame.emplace([] { return 0; }).name("dynamic::start loop").precede(dyn_output_task);

        auto dyn_input_func = [this](tf::Subflow &sbf) {
            for (auto &&[model_type, handle] : mm.dynamicModels) {
                sbf.emplace(ModelInputFunc{*this, handle.obj, model_type});
            }
            sbf.join();
        };
        auto dyn_input_task = frame.emplace(dyn_input_func).name("dynamic::input").succeed(collect_task);

        auto dyn_tick_func = [this](tf::Subflow &sbf) {
            for (auto &&[model_type, handle] : mm.dynamicModels) {
                sbf.emplace(ModelTickFunc{*this, handle.obj, model_type});
            }
            sbf.join();
        };
        auto dyn_tick_task = frame.emplace(dyn_tick_func).name("dynamic::tick").succeed(dyn_input_task);

        auto dyn_loop_condition = frame.emplace([this] { return s.loop != 0 ? 0 : 1; })
                                      .name("dynamic::next frame condition")
                                      .precede(dyn_output_task)
                                      .succeed(dyn_tick_task);

        // static tasks
        tm.buffer.output_buffer.resize(mm.models.size());

        // std::map<std::string, std::set<std::string>> targets;
        // for (auto &&[model_type, topics] : tm.topics) {
        //     for (auto &&topic : topics) {
        //         targets[model_type].merge(topic.getTargets());
        //     }
        // }

        for (auto &&[model_id, model_entity] : std::views::enumerate(mm.models)) {
            auto &&[model_type, model_info] = model_entity;
            auto it = tm.topics.find(model_type);

            auto output_task =
                frame
                    .emplace(ModelOutputFunc{*this, model_info.obj, model_type, model_info.outputDataMovable,
                                             tm.buffer.output_buffer[model_id], it, it == tm.topics.end()})
                    .name(std::format("{}[{}]::output", model_type, model_info.obj->GetID()))
                    .precede(collect_task);

            // // find dependencies
            // for (auto &&target : targets[model_type]) {
            //     tm.dependenciesOfTarget[target].push_back(model_id);
            //     if (auto it = collector.find(target); it != collector.end()) {
            //         output_task.precede(it->second);
            //     } else {
            //         output_task.precede(dyn_collector);
            //     }
            // }

            auto init_task = frame.emplace([] { return 0; })
                                 .name(std::format("{}[{}]::start loop", model_type, model_info.obj->GetID()))
                                 .precede(output_task);

            auto input_task = frame.emplace(ModelInputFunc{*this, model_info.obj, model_type})
                                  .name(std::format("{}[{}]::input", model_type, model_info.obj->GetID()))
                                  .succeed(collect_task);

            auto tick_task = frame.emplace(ModelTickFunc{*this, model_info.obj, model_type})
                                 .name(std::format("{}[{}]::tick", model_type, model_info.obj->GetID()))
                                 .succeed(input_task);

            auto loop_condition_func = [this] { return s.loop != 0 ? 0 : 1; };
            auto loop_condition =
                frame.emplace(loop_condition_func)
                    .name(std::format("{}[{}]::next frame condition", model_type, model_info.obj->GetID()))
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