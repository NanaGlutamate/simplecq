#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <vector>

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
std::expected<std::invoke_result_t<Func>, std::string> doWithCatch(Func &&fun) noexcept {
    try {
        if constexpr (!std::is_same_v<void, std::invoke_result_t<Func>>) {
            return fun();
        } else {
            fun();
            return {};
        }
    } catch (std::exception &err) {
        return std::unexpected(err.what());
    } catch (...) {
        return std::unexpected("[unhandled exception]");
    }
}

} // namespace

struct ModelLoader {
    std::unordered_map<std::string, ModelDllInterface> dlls;
    std::unordered_map<std::string, bool> movable;
    std::tuple<std::string, ModelObjHandle> loadModel(const std::string &type) {
        auto model = ::loadModel(dlls[type]);
        model.outputDataMovable = movable[type];
        return {type, std::move(model)};
    }
    std::expected<void, std::string> loadDll(const std::string &name, const std::string &path, bool move) {
        auto ans = ::loadDll(path);
        if (!ans) {
            return std::unexpected(std::string(ans.error()));
        }
        dlls[name] = ans.value();
        movable[name] = move;
        return std::expected<void, std::string>{};
    }
};

struct TinyCQ {
    using CSValueMap = std::unordered_map<std::string, std::any>;
    using ModelType = std::string;

    ModelLoader loader;

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
    // std::unordered_map<std::string, std::vector<ModelObjHandle>> models;
    std::vector<std::tuple<std::string, ModelObjHandle>> models;
    // TODO: dynamic created model
    std::vector<std::tuple<std::string, ModelObjHandle>> dynamicModels;

    struct CreateModelCommand {
        uint64_t ID;
        uint16_t sideID;
        CSValueMap param;
        std::string type;
    };
    std::vector<CreateModelCommand> createModelCommands;

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

    // src type -> sent topics
    std::unordered_map<std::string, std::vector<TopicInfo>> topics;

    struct Buffers {
        template <typename Ty> struct alignas(128) Padding : public Ty {};
        // model(src) -> model_type_name(target) -> topics
        std::vector<Padding<std::unordered_map<std::string, std::vector<CSValueMap>>>> output_buffer;
        // model_type_name -> received topics
        std::unordered_map<std::string, std::vector<CSValueMap>> topic_buffer;
        size_t loop;
        // dynamic
        std::vector<Padding<std::unordered_map<std::string, std::vector<CSValueMap>>>> dyn_output_buffer;
    } buffer{{}, {}, 0, {}};

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

    void writeLog(std::string_view src, std::string_view msg, int32_t level) noexcept {
        if (level < cfg.log_level || !cfg.enable_log || !cfg.log_file)
            return;
        std::unique_lock lock{io_lock};
        cfg.log_file << std::format("[{}-{}]: {}", src, level, msg) << std::endl;
    }

    std::string commonCallBack(const std::string &type, const std::unordered_map<std::string, std::any> &param) {
        // TODO: dynamic create entity
        using namespace std::literals;
        if (type == "CreateEntity"sv) {
            uint64_t ID = std::any_cast<uint64_t>(param.find("ID")->second);
            uint16_t sideID = std::any_cast<uint16_t>(param.find("ForceSideID")->second);
            std::string type = std::any_cast<std::string>(param.find("ModelID")->second);
            createModelCommands.push_back({ID, sideID, param, std::move(type)});
        } else {
            writeLog("Engine",
                     std::format("Unsupport Callback function call: {}({})", type,
                                 tools::myany::printCSValueMapToString(param)),
                     5);
        }
        return "";
    }

    void setModelState(ModelObjHandle &model, uint64_t ID, uint16_t sideID, std::string type) {
        model.obj->SetID(ID);
        model.obj->SetForceSideID(sideID);
        model.obj->SetLogFun(
            [this, type{std::move(type)}](const std::string &msg, uint32_t level) { writeLog(type, msg, level); });
        model.obj->SetCommonCallBack(
            [this](const std::string &type, const std::unordered_map<std::string, std::any> &param) {
                commonCallBack(type, param);
                return std::string{};
            });
    }

    std::expected<void, std::string> init(const std::string &config_file) {
        auto config = YAML::LoadFile(config_file);
        for (auto &&n : config["model_types"]) {
            auto succ = loader.loadDll(n["model_type_name"].as<std::string>(), n["dll_path"].as<std::string>(),
                                       n["output_movable"].as<bool>(false));
            if (!succ) {
                return std::unexpected(succ.error());
            }
        }
        for (auto &&n : config["models"]) {
            auto type = n["model_type"].as<std::string>();
            auto &model = std::get<1>(models.emplace_back(loader.loadModel(type)));
            setModelState(model, n["id"].as<uint64_t>(), n["side_id"].as<uint16_t>(), type);
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

            auto res = doWithCatch([&] {
                if (model.obj->Init(value)) {
                    throw std::logic_error("init function return false");
                }
            });
            if (!res) {
                return std::unexpected(std::format("Exception When Model[{}]Init: {}", type, res.error()));
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
        buffer.output_buffer.resize(models.size());

        // // TODO: parallel between subscribers
        // // target -> task
        // std::unordered_map<std::string, tf::Task> collector;
        // // src -> target
        // std::unordered_map<std::string, std::vector<std::string>> dependencies;
        // for(auto &&[src, topic_list] : topics) {
        //     for(auto&& t : topic_list) {
        //         for(auto&& [target, _] : t.trans.rules) {

        //         }
        //     }
        // }

        auto collect_task = frame
                                .emplace([this] {
                                    collectTopic();
                                    buffer.loop--;
                                })
                                .name("collect output");

        {
            tf::Task output_task =
                frame
                    .emplace([this](tf::Subflow &sbf) {
                        // create model
                        for (auto &&[ID, sideID, param, type] : createModelCommands) {
                            auto &model = std::get<1>(dynamicModels.emplace_back(loader.loadModel(type)));
                            setModelState(model, ID, sideID, type);
                            auto res = doWithCatch([&] {
                                if (model.obj->Init(param)) {
                                    throw std::logic_error("init function return false");
                                }
                            });
                            if (!res) {
                                dynamicModels.pop_back();
                                writeLog("Engine", std::format("Exception When Create Dynamic Model: {}", res.error()),
                                         5);
                            }
                        }
                        createModelCommands.clear();
                        // output
                        buffer.dyn_output_buffer.resize(dynamicModels.size());
                        for (auto &&[idx, data] : std::views::enumerate(dynamicModels)) {
                            auto &&[model_type, handle] = data;
                            sbf.emplace([this, obj{handle.obj}, model_type, movable{handle.outputDataMovable},
                                         &ret{buffer.dyn_output_buffer[idx]}] {
                                auto it = topics.find(model_type);
                                CSValueMap *model_output_ptr;
                                doWithCatch([&] {
                                    model_output_ptr = obj->GetOutput();
                                }).or_else([&, this](const std::string &err) -> std::expected<void, std::string> {
                                    writeLog("Engine", std::format("Exception When Dynamic Model Output: {}", err), 5);
                                    return {};
                                });
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
                            });
                        }
                        sbf.join();
                    })
                    .name(std::format("dynamic::output"))
                    .precede(collect_task);
            tf::Task init_task = frame.emplace([] { return 0; }).name("dynamic::start loop").precede(output_task);
            tf::Task input_task =
                frame
                    .emplace([this](tf::Subflow &sbf) {
                        for (auto &&[model_type, handle] : dynamicModels) {
                            sbf.emplace([this, obj{handle.obj}, model_type] {
                                if (auto it = buffer.topic_buffer.find(model_type); it != buffer.topic_buffer.end()) {
                                    for (auto &&v : it->second) {
                                        try {
                                            obj->SetInput(v);
                                        } catch (std::exception &err) {
                                            writeLog("Engine",
                                                     std::format("Exception When Dynamic Model Input: {}\n{}",
                                                                 err.what(), tools::myany::printCSValueMapToString(v)),
                                                     5);
                                        }
                                    }
                                }
                            });
                        }
                        sbf.join();
                    })
                    .name("dynamic::input")
                    .succeed(collect_task);
            tf::Task tick_task =
                frame
                    .emplace([this](tf::Subflow &sbf) {
                        for (auto &&[model_type, handle] : dynamicModels) {
                            sbf.emplace([this, obj{handle.obj}] {
                                try {
                                    obj->Tick(cfg.dt);
                                } catch (std::exception &err) {
                                    writeLog("Engine", std::format("Exception When Dynamic Model Tick: {}", err.what()),
                                             5);
                                }
                            });
                        }
                        sbf.join();
                    })
                    .name("dynamic::tick")
                    .succeed(input_task);

            tf::Task loop_condition = frame.emplace([this] { return buffer.loop != 0 ? 0 : 1; })
                                          .name("dynamic::next frame condition")
                                          .precede(output_task)
                                          .succeed(tick_task);
        }

        size_t model_id = 0;
        for (auto &&[model_type, model_info] : models) {
            auto it = topics.find(model_type);
            auto output_task =
                frame
                    .emplace([this, model_type, obj{model_info.obj}, &ret{buffer.output_buffer[model_id]}, it,
                              movable{model_info.outputDataMovable}] {
                        CSValueMap *model_output_ptr;
                        doWithCatch([&] {
                            model_output_ptr = obj->GetOutput();
                        }).or_else([&, this](const std::string &err) -> std::expected<void, std::string> {
                            writeLog("Engine", std::format("Exception When Model[{}]Output: {}", model_type, err.what()), 5);
                            return {};
                        });
                        if (it == topics.end()) {
                            return;
                        }
                        for (auto &&topic : it->second) {
                            if (!topic.containsAllMember(*model_output_ptr)) {
                                continue;
                            }
                            std::array<TransformInfo::InputBuffer, 1> buffer{{model_type, model_output_ptr, movable}};
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
                                    writeLog("Engine",
                                             std::format("Exception When Model[{}]Input: {}\n{}", model_type,
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
                            writeLog("Engine", std::format("Exception When Model[{}]Tick: {}", model_type, err.what()),
                                     5);
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
    // // target -> model_id
    // std::unordered_map<std::string, std::vector<size_t>> ;

    void collectTopic() {
        // TODO: parallel
        for (auto &&[target, topics] : buffer.topic_buffer) {
            topics.clear();
        }
        std::array output{buffer.output_buffer, buffer.dyn_output_buffer};
        for (auto &&f : output | std::views::join) {
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

struct CommandReceiver {
    TinyCQ &cq;
    bool processCommand(std::string_view command) {}
    void replMode() {
        for (;;) {
            std::string command;
            std::cout << ">" << std::endl;
            std::cin >> command;
            if (!processCommand(command)) {
                return;
            }
        }
    }
};

int main() {
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
    return 0;
}
