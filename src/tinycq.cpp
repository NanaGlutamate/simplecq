#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <ranges>
#include <unordered_map>
#include <vector>
#include <type_traits>

#include <taskflow/taskflow.hpp>
#include <yaml-cpp/yaml.h>

#include "csmodel_base.h"
#include "datatransform.hpp"
#include "dllop.hpp"
#include "parseany.hpp"

namespace {

auto mymin(auto a, auto b) { return a < b ? a : b; }

auto mymax(auto a, auto b) { return a > b ? a : b; }

template <std::invocable<> Func>
std::expected<std::invoke_result_t<Func>, std::string> doWithCatch(Func&& fun) noexcept {
    try{
        return fun();
    }catch(std::exception& err){
        return std::unexpected(err.what());
    }catch(...){
        return std::unexpected("unhandled exception");
    }
}

} // namespace

struct TinyCQ {
    using CSValueMap = std::unordered_map<std::string, std::any>;
    struct Config {
        // ms
        double dt;
        size_t log_level;
        bool enable_log;
        std::ofstream log_file;
    } cfg{100, 0, true, {}};
    struct Info {
        // ms
        double fps;
    } info{0.};

    struct Scene {
        double x_lower = 0., x_upper = 0., y_lower = 0., y_upper = 0.;
        struct EntityState {
            bool destroied;
            uint16_t ForceSideID;
            double longitude, latitude;
        };
        std::vector<EntityState> buffer;
        void addEntity(uint16_t state, uint16_t ForceSideID, double longitude, double latitude) {
            if (x_lower == 0.) {
                x_lower = x_upper = longitude;
                y_lower = y_upper = latitude;
            }
            double x_edge = 0.2 * (x_upper - x_lower) + 0.0001;
            double y_edge = 0.2 * (y_upper - y_lower) + 0.0001;
            x_lower = mymin(x_lower, longitude - x_edge);
            x_upper = mymax(x_upper, longitude + x_edge);
            y_lower = mymin(y_lower, latitude - y_edge);
            y_upper = mymax(y_upper, latitude + y_edge);
            buffer.emplace_back(state != 3, ForceSideID, longitude, latitude);
        }
        void draw(int line, int row = -1) {
            if (row == -1) {
                row = int(ceil((x_upper - x_lower) / (y_upper - y_lower) * line / 2));
                if (row > 150) {
                    row = 150;
                }
            }
            std::cout << tools::mystr::repeat("-", row + 2) << "\n";
            double x_diff = (x_upper - x_lower) / row;
            double y_diff = (y_upper - y_lower) / line;
            for (size_t l = 0; l < line; ++l) {
                std::string base = tools::mystr::repeat(" ", row);
                for (auto &&[d, f, lon, lat] :
                     buffer | std::views::filter([l, y_diff, y_upper{this->y_upper}](auto &ele) {
                         auto lat = ele.latitude;
                         return (lat < y_upper - y_diff * l) && (lat >= y_upper - y_diff * (l + 1));
                     })) {
                    base[size_t(floor((lon - x_lower) / x_diff))] = (d ? 'x' : (f == 1 ? '*' : 'o'));
                }
                std::cout << "|" << base << "|\n";
            }
            std::cout << tools::mystr::repeat("-", row + 2) << "\n";
            buffer.clear();
        }
    } s;

  private:
    std::mutex io_lock;
    tf::Executor executor;
    tf::Taskflow frame;
    // model_type_name -> models
    std::unordered_map<std::string, std::vector<ModelInfo>> models;
    // TODO: dynamic created model
    // TODO: vector<tuple<string, ModelInfo>>

    struct TopicInfo {
        std::vector<std::string> members;
        TransformInfo trans;
        bool containsAllMember(const CSValueMap &data) {
            for (auto &&name : members) {
                if (!data.contains(name)) {
                    return false;
                }
            }
            return true;
        }
    };
    // model_type_name -> topics
    std::unordered_map<std::string, std::vector<TopicInfo>> topics;
    struct Buffers {
        size_t loop;
        // model -> model_type_name -> topics
        std::vector<std::unordered_map<std::string, std::vector<CSValueMap>>> output_buffer;
        // model_type_name -> received topics
        std::unordered_map<std::string, std::vector<CSValueMap>> topic_buffer;
    } buffer{0, {}, {}};

  public:
    TinyCQ() = default;

    TinyCQ(const TinyCQ &) = delete;

    void clear() {
        frame.clear();
        // input_tick.clear();
        // tick.clear();
        // dissambledTopics.clear();
        models.clear();
        topics.clear();
        buffer.loop = 0;
        buffer.output_buffer.clear();
        buffer.topic_buffer.clear();
    }

    void writeLog(const std::string &msg, int32_t level) {
        if (level < cfg.log_level || !cfg.enable_log)
            return;
        std::unique_lock lock{io_lock};
        cfg.log_file << std::format("[{}]: {}", level, msg) << std::endl;
    }

    std::string commonCallBack(const std::string &type, const std::unordered_map<std::string, std::any> &param) {
        // TODO: dynamic create entity
    }

    std::expected<void, std::string> init(const std::string &config_file) {
        std::unordered_map<std::string, ModelDllInterface> dlls;
        std::unordered_map<std::string, bool> movable;
        auto config = YAML::LoadFile(config_file);
        for (auto &&n : config["model_types"]) {
            auto ans = loadDll(n["dll_path"].as<std::string>());
            if (!ans) {
                return std::unexpected(std::string(ans.error()));
            }
            dlls[n["model_type_name"].as<std::string>()] = ans.value();
            movable[n["model_type_name"].as<std::string>()] = n["output_movable"].as<bool>(false);
        }
        for (auto &&n : config["models"]) {
            auto type = n["model_type"].as<std::string>();
            models[type].emplace_back(loadModel(dlls[type]));
            auto &model = models[type].back();
            model.outputDataMovable = movable[type];
            model.obj->SetID(n["id"].as<uint64_t>());
            model.obj->SetForceSideID(n["side_id"].as<uint16_t>());
            model.obj->SetLogFun([this, type](const std::string &msg, uint32_t level) {
                writeLog(std::format("[{}]: {}", type, msg), level);
            });
            auto ans = tools::myany::parseXMLString(n["init_value"].as<std::string>());
            if (!ans) {
                return std::unexpected(
                    std::format("error when parse \"{}\" : {}", n["init_value"].as<std::string>(), ans.error()));
            }
            if (ans.value().type() != typeid(CSValueMap)) {
                return std::unexpected(
                    std::format("error when parse \"{}\" : must be CSValueMap", n["init_value"].as<std::string>()));
            }
            auto value = std::any_cast<CSValueMap>(ans.value());
            value.emplace("ForceSideID", model.obj->GetForceSideID());
            value.emplace("ID", model.obj->GetID());

            try {
                model.obj->Init(value);
            } catch (std::exception &err) {
                clear();
                return std::unexpected(std::format("Exception When Model[{}]Init: {}", type, err.what()));
            } catch (...) {
                clear();
                return std::unexpected(std::format("Unhandled Exception When Model[{}]Init", type));
            }
        }
        for (auto &&n : config["topics"]) {
            TransformInfo trans;
            auto from = n["from"].as<std::string>();
            for (auto &&sub : n["subscribers"]) {
                auto to = sub["to"].as<std::string>();
                for (auto &&convert : sub["name_convert"]) {
                    auto src = convert["name"].as<std::string>(convert["src_name"].as<std::string>(""));
                    auto dst = convert["name"].as<std::string>(convert["dst_name"].as<std::string>(""));
                    trans.rules[from][src].push_back({to, std::move(dst)});
                }
            }
            topics[from].push_back(TopicInfo{n["members"].as<std::vector<std::string>>(), std::move(trans)});
        }
        return std::expected<void, std::string>();
    }

    void buildGraph() {
        size_t model_count = 0;
        for (auto &&[n, l] : models) {
            model_count += l.size();
        }
        buffer.output_buffer.resize(model_count);

        // TODO: parallel between describers
        // // topic -> task
        // std::unordered_map<std::string, tf::Task> collector;
        // std::unordered_map<std::string, std::string> topic_dependencies;
        // for(auto &&[_, tl] : topics) {
        //     for(auto&& t : tl) {
        //         for(auto&& [target, _] : t.trans.rules) {
        //             collector.emplace(target, {});
        //         }
        //     }
        // }

        // TODO: dynamic created model
        auto collect_task = frame
                                .emplace([this] {
                                    collectTopic();
                                    buffer.loop--;
                                })
                                .name("collect output");

        size_t model_id = 0;
        for (auto &&[model_type, model_list] : models) {
            for (auto &&model_info : model_list) {
                auto it = topics.find(model_type);
                auto output_task =
                    frame
                        .emplace([this, model_type, obj{model_info.obj}, movable{model_info.outputDataMovable},
                                  &ret{buffer.output_buffer[model_id]}, it] {
                            CSValueMap *model_output_ptr;
                            try {
                                model_output_ptr = obj->GetOutput();
                            } catch (std::exception &err) {
                                writeLog(std::format("Exception When Model[{}]Output: {}", model_type, err.what()), 5);
                                return;
                            } // catch (...) {}
                            if (it == topics.end()) {
                                return;
                            }
                            for (auto &&topic : it->second) {
                                if (!topic.containsAllMember(*model_output_ptr)) {
                                    continue;
                                }
                                std::array<TransformInfo::InputBuffer, 1> buffer{
                                    {model_type, model_output_ptr, movable}};
                                for (auto &&[n, v] : topic.trans.transform(std::span{buffer})) {
                                    ret[n].push_back(std::move(v));
                                }
                            }
                        })
                        .name(std::format("{}[{}]::output", model_type, model_info.obj->GetID()))
                        .precede(collect_task);

                auto init_task = frame.emplace([] { return 0; })
                                     .name(std::format("{}[{}]::start loop", model_type, model_info.obj->GetID()))
                                     .precede(output_task);

                auto input_task =
                    frame
                        .emplace([this, model_type, obj{model_info.obj}] {
                            if (auto it = buffer.topic_buffer.find(model_type); it != buffer.topic_buffer.end()) {
                                for (auto &&v : it->second) {
                                    try {
                                        obj->SetInput(v);
                                    } catch (std::exception &err) {
                                        writeLog(std::format("Exception When Model[{}]Input: {}\n{}", model_type,
                                                             err.what(), tools::myany::printCSValueMapToString(v)),
                                                 5);
                                    }
                                }
                            }
                        })
                        .name(std::format("{}[{}]::input", model_type, model_info.obj->GetID()))
                        .succeed(collect_task);

                auto tick_task =
                    frame
                        .emplace([this, obj{model_info.obj}, model_type] {
                            try {
                                obj->Tick(this->cfg.dt);
                            } catch (std::exception &err) {
                                writeLog(std::format("Exception When Model[{}]Tick: {}", model_type, err.what()), 5);
                            }
                        })
                        .name(std::format("{}[{}]::tick", model_type, model_info.obj->GetID()))
                        .succeed(input_task);

                auto loop_condition =
                    frame.emplace([this] { return buffer.loop != 0 ? 0 : 1; })
                        .name(std::format("{}[{}]::next frame condition", model_type, model_info.obj->GetID()))
                        .precede(output_task)
                        .succeed(tick_task);

                model_id += 1;
            }
        }
    }

    void run(size_t times = 1) {
        buffer.loop = times;
        auto p = std::chrono::high_resolution_clock::now();
        executor.run(frame).wait();
        auto t = std::chrono::high_resolution_clock::now() - p;
        double t2 = double(std::chrono::duration_cast<std::chrono::microseconds>(t).count());
        info.fps =
            double(times) / (t2 * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den);
    }

    void printBuffer() {
        for (auto &&[tar, l] : buffer.topic_buffer) {
            std::cout << tar << ":" << std::endl;
            for (auto &&cs : l) {
                std::cout << "\t";
                tools::myany::printCSValueMap(cs);
            }
        }
    }

    void draw() {
        if (auto it = buffer.topic_buffer.find("root"); it != buffer.topic_buffer.end()) {
            for (auto &&lonlat : it->second) {
                s.addEntity(std::any_cast<uint16_t>(lonlat.find("State")->second),
                            std::any_cast<uint16_t>(lonlat.find("ForceSideID")->second),
                            std::any_cast<double>(lonlat.find("longitude")->second),
                            std::any_cast<double>(lonlat.find("latitude")->second));
            }
            std::cout << std::format("fps: {} rate: {}\n\n", info.fps, info.fps * cfg.dt / 1000);
            s.draw(12);
        }
    }

  private:
    void collectTopic() {
        for (auto &&[target, topics] : buffer.topic_buffer) {
            topics.clear();
        }
        for (auto &&f : buffer.output_buffer) {
            for (auto &&[k, v] : f) {
                buffer.topic_buffer[k].insert_range(buffer.topic_buffer[k].end(), std::move(v));
                v.clear();
            }
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
    // using namespace cq;

    TinyCQ cq;
    cq.cfg.log_file = std::ofstream(__PROJECT_ROOT_PATH "/config/log.txt");
    cq.cfg.log_level = 4;
    cq.init(__PROJECT_ROOT_PATH "/config/scene_carbattle.yml");
    cq.buildGraph();
    // cq.frame.dump(std::cout);
    for (int i = 0;; ++i) {
        cq.run(600);
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
