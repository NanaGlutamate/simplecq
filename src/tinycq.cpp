#include <format>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <taskflow/taskflow.hpp>
#include <yaml-cpp/yaml.h>

#include "csmodel_base.h"
#include "datatransform.hpp"
#include "dllop.hpp"
#include "parseany.hpp"
#include "thread_pool.hpp"

namespace {

std::mutex io_lock;

}

struct TinyCQ {
    using CSValueMap = std::unordered_map<std::string, std::any>;
    struct TopicInfo {
        std::vector<std::string> members;
        TransformInfo trans;
    };
    struct Config {
        // ms
        double dt;
        size_t log_level;
        bool enable_log;
    } cfg{100, 0, true};
    tf::Executor executor;
    tf::Taskflow frame, input, tick;
    // model_type_name -> models
    std::unordered_map<std::string, std::vector<ModelInfo>> models;
    // model_type_name -> topics
    std::unordered_map<std::string, std::vector<TopicInfo>> topics;
    // model_type_name -> received topics
    std::unordered_map<std::string, std::vector<CSValueMap>> topicBuffer;
    TinyCQ() = default;
    TinyCQ(const TinyCQ &) = delete;
    void clear() {
        frame.clear();
        input.clear();
        tick.clear();
        models.clear();
        topics.clear();
    }
    void writeLog(const std::string &msg, int32_t level) {
        if (level < cfg.log_level || !cfg.enable_log)
            return;
        std::unique_lock lock{io_lock};
        std::cout << std::format("[{}]: {}", level, msg) << std::endl;
    }
    std::expected<void, std::string> init(const std::string &configFile) {
        std::unordered_map<std::string, ModelDllInterface> dlls;
        auto config = YAML::LoadFile(configFile);
        for (auto &&n : config["model_types"]) {
            auto ans = loadDll(n["dll_path"].as<std::string>());
            if (!ans) {
                return std::unexpected(std::string(ans.error()));
            }
            dlls[n["model_type_name"].as<std::string>()] = ans.value();
        }
        for (auto &&n : config["models"]) {
            auto type = n["model_type"].as<std::string>();
            models[type].emplace_back(loadModel(dlls[type]));
            auto &model = models[type].back();
            model.obj->SetID(n["id"].as<uint64_t>());
            model.obj->SetForceSideID(n["side_id"].as<uint16_t>());
            model.obj->SetLogFun([this, type](const std::string &msg, uint32_t level) {
                writeLog(std::format("[{}]: {}", type, msg), level);
            });
            auto ans = tools::myany::parseXMLString(n["init_value"].as<std::string>());
            if (!ans) {
                return std::unexpected(
                    std::format("error when parse {} : {}", n["init_value"].as<std::string>(), ans.error()));
            }
            if (ans.value().type() != typeid(CSValueMap)) {
                return std::unexpected(
                    std::format("error when parse {} : must be CSValueMap", n["init_value"].as<std::string>()));
            }
            auto value = std::any_cast<CSValueMap>(ans.value());
            value.emplace("ForceSideID", model.obj->GetForceSideID());
            value.emplace("ID", model.obj->GetID());
            model.obj->Init(value);
        }
        for (auto &&n : config["topics"]) {
            TransformInfo trans;
            auto from = n["from"].as<std::string>();
            for (auto &&sub : n["subscribers"]) {
                auto to = sub["to"].as<std::string>();
                for (auto &&convert : sub["name_convert"]) {
                    auto src =
                        convert["name"] ? convert["name"].as<std::string>() : convert["src_name"].as<std::string>();
                    auto dst =
                        convert["name"] ? convert["name"].as<std::string>() : convert["dst_name"].as<std::string>();
                    trans.rules[from][src].push_back({to, std::move(dst)});
                }
            }
            topics[from].push_back(TopicInfo{n["members"].as<std::vector<std::string>>(), std::move(trans)});
        }
        return std::expected<void, std::string>();
    }
    void buildGraph() {
        for (auto &&[model_type, model_list] : models) {
            for (auto &&model_info : model_list) {
                input.emplace([this, model_type, obj{model_info.obj}] {
                    if (auto it = topicBuffer.find(model_type); it != topicBuffer.end()) {
                        for (auto &&v : it->second) {
                            try {
                                obj->SetInput(v);
                            } catch (std::exception &err) {
                                writeLog(std::format("Model[{}]Input: {}\n{}", model_type, err.what(),
                                                     tools::myany::printCSValueMapToString(v)),
                                         5);
                            }
                        }
                    }
                });

                tick.emplace([this, obj{model_info.obj}, model_type] {
                    try {
                        obj->Tick(this->cfg.dt);
                    } catch (std::exception &err) {
                        writeLog(std::format("Model[{}]Tick: {}", model_type, err.what()), 5);
                    }
                });
            }
        }

        auto output = [this](tf::Subflow &sf) {
            using transformed_t = std::unordered_map<std::string, CSValueMap>;
            std::vector<std::future<transformed_t>> futures;
            for (auto &&[model_type, model_list] : this->models) {
                auto it = topics.find(model_type);
                if (it == topics.end()) {
                    continue;
                }
                for (auto &&model_info : model_list) {
                    CSValueMap *model_output_ptr;
                    try {
                        model_output_ptr = model_info.obj->GetOutput();
                    } catch (std::exception &err) {
                        writeLog(std::format("Model[{}]Outout: {}", model_type, err.what()), 5);
                    }
                    for (auto &&topic : it->second) {
                        futures.push_back(sf.async([model_type, &topic, model_output_ptr]() -> transformed_t {
                            for (auto &&name : topic.members) {
                                if (!model_output_ptr->contains(name)) {
                                    return {};
                                }
                            }
                            std::array<TransformInfo::InputBuffer, 1> buffer{
                                {std::move(model_type), model_output_ptr, false}};
                            return topic.trans.transform(std::span{buffer});
                        }));
                    }
                }
            }
            topicBuffer.clear();
            sf.join();
            for (auto &&f : futures) {
                for (auto &&[k, v] : f.get()) {
                    topicBuffer[k].push_back(std::move(v));
                }
            }
        };

        auto o_task = frame.emplace(output).name("output");
        auto i_task = frame.composed_of(input).name("input");
        auto t_task = frame.composed_of(tick).name("tick");

        o_task.precede(i_task);
        i_task.precede(t_task);
    }
    void run(size_t times = 1) { executor.run_n(frame, times).wait(); }
    void printBuffer() {
        for (auto &&[tar, l] : topicBuffer) {
            std::cout << tar << ":" << std::endl;
            for (auto &&cs : l) {
                std::cout << "\t";
                tools::myany::printCSValueMap(cs);
            }
        }
    }
    void draw() {
        struct XY {
            uint64_t ID;
            double longitude, latitude;
        };
        std::vector<XY> xy;
        if (auto it = topicBuffer.find("root"); it != topicBuffer.end()) {
            for (auto lonlat : it->second) {
                xy.push_back(XY{
                    std::any_cast<uint64_t>(lonlat.find("ID")->second),
                    std::any_cast<double>(lonlat.find("longitude")->second),
                    std::any_cast<double>(lonlat.find("latitude")->second),
                });
            }
            std::sort(xy.begin(), xy.end(), [](auto &x, auto &y) { return x.ID < y.ID; });
            std::cout << "----------" << std::endl;
            for (auto &&[id, lon, lat] : xy) {
                std::cout << std::format("{}\t({}, {})", id, lon, lat) << std::endl;
            }
            std::cout << "----------" << std::endl;
        }
    }
};

// namespace {
// std::unique_ptr<TinyCQ> cqInstance;
// } // namespace

// extern "C" {
// __declspec(dllexport) bool CreateCQInstance() {
//     if (cqInstance) {
//         return false;
//     }
//     cqInstance = std::make_unique<TinyCQ>();
//     return true;
// }
// __declspec(dllexport) bool SendMessage(int id, const char *msg, size_t msgLen, const char *topicName,
//                                        size_t topicNameLen) {
//     return true;
// }
// }

int main() {
    using namespace cq;

    TinyCQ cq;
    cq.init(__PROJECT_ROOT_PATH "/config/scene_carbattle.yml");
    cq.buildGraph();
    cq.cfg.log_level = 1;
    for (;;) {
        cq.run(10);
        // tools::myany::printCSValueMap(cq.topicBuffer);
        // cq.printBuffer();
        cq.draw();
    }

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
