// #include <fkYAML/node.hpp>
#include <yaml-cpp/yaml.h>

#include <array>
#include <format>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include <assert.h>

#include "csmodel_base.h"
#include "dllop.hpp"
#include "profile.hpp"

namespace {

using namespace std::literals;

using CSValueMap = std::unordered_map<std::string, std::any>;

} // namespace

struct ModelInfo {
    // ModelInfo(const ModelInfo &) = delete;
    // ModelInfo &operator=(const ModelInfo &) = delete;
    //~ModelInfo() {
    //     if (obj) {
    //         dll.destoryFunc(obj, false);
    //     }
    //     obj = nullptr;
    // }
    ModelDllInterface dll;
    CSModelObject *obj;
    bool outputDataMovable = false;
};

std::expected<ModelInfo, std::string_view> loadModel(const std::string &dllName) {
    return loadDll(dllName).and_then([](ModelDllInterface dll) -> std::expected<ModelInfo, std::string_view> {
        ModelInfo info;
        info.dll = dll;
        info.obj = dll.createFunc();
        return info;
    });
}

struct TransformInfo {
    struct Action {
        std::string to, dstName;
    };
    struct InputBuffer {
        std::string name;
        CSValueMap *buffer;
        bool movable;
    };
    // from, srcname
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Action>>> rules;
    std::unordered_map<std::string, CSValueMap> transform(std::span<InputBuffer> buffers) {
        std::unordered_map<std::string, CSValueMap> ret;
        for (auto &&[bufferName, data, movable] : buffers) {
            if (auto it = rules.find(bufferName); it != rules.end()) {
                for (auto &&[name, value] : *data) {
                    if (auto it2 = it->second.find(name); it2 != it->second.end()) {
                        auto &actions = it2->second;
                        assert(actions.size());
                        // if (actions.empty()) {
                        //     continue;
                        // }
                        auto size = actions.size();
                        if (movable) {
                            for (size_t i = 0; i < size - 1; ++i) {
                                ret[actions[i].to][actions[i].dstName] = value;
                            }
                            ret[actions[size - 1].to][actions[size - 1].dstName] = std::move(value);
                        } else {
                            for (size_t i = 0; i < size; ++i) {
                                ret[actions[i].to][actions[i].dstName] = value;
                            }
                        }
                    }
                }
            }
        }
        return ret;
    }
};

class MyAssembledModel : public CSModelObject {
  public:
    // init: root->subs
    // input: root->subs
    // output: subs->subs, subs->root
    MyAssembledModel() = default;
    virtual bool Init(const std::unordered_map<std::string, std::any> &value) override {
        // auto location = getLibDir();
        // std::ifstream configFile(location + "assemble.yml");
        // fkyaml::node config = fkyaml::node::deserialize(configFile);
        // for (auto &&model : config["models"]) {
        //     auto &name = model["model_name"].get_value_ref<std::string &>();
        //     subModels.emplace(name, location + model["dll_name"].get_value_ref<std::string &>());
        //     if (model.contains("output_movable")) {
        //         subModels.find(name)->second.outputDataMovable = model["output_movable"].get_value<bool>();
        //     }
        // }
        // auto load = [&config](TransformInfo &tar, std::string_view name, auto check) {
        //     for (auto &&rule : config[name]) {
        //         bool containsName = rule.contains("name");
        //         bool containsFrom = rule.contains("from");
        //         bool containsTo = rule.contains("to");
        //         const auto &srcName = containsName ? rule["name"].get_value_ref<std::string &>()
        //                                            : rule["src_name"].get_value_ref<std::string &>();
        //         const auto &dstName = containsName ? rule["name"].get_value_ref<std::string &>()
        //                                            : rule["dst_name"].get_value_ref<std::string &>();
        //         static std::string rootName = "root";
        //         const auto &from = rule.contains("from") ? rule["from"].get_value_ref<std::string &>() : rootName;
        //         const auto &to = rule.contains("to") ? rule["to"].get_value_ref<std::string &>() : rootName;
        //         check(from, to);
        //         tar.rules[from][srcName].push_back(TransformInfo::Action{
        //             .to = to,
        //             .dstName = dstName,
        //         });
        //     }
        // };
        if (!log_) {
            log_ = [](auto msg, auto) { std::cout << std::format("[AssembleModel]: ", msg) << std::endl; };
        }

        auto location = getLibDir();
        // std::ifstream configFile(location + "assemble.yml");
        auto configFile = location + "assemble.yml";
        auto config = YAML::LoadFile(configFile);
        if (config["config"]) {
            auto n = config["config"];
            if (n["profile"]) {
                profile = n["profile"].as<std::string>();
            }
        }
        // fkyaml::node config = fkyaml::node::deserialize(configFile);
        for (auto &&model : config["models"]) {
            auto name = model["model_name"].as<std::string>();
            auto modelObj = loadModel(location + model["dll_name"].as<std::string>());
            if (!modelObj.has_value()) {
                auto errorInfo = std::format("[AssembledModel] error when loading [{}]: ", name);
                errorInfo += modelObj.error();
                WriteLog(errorInfo, 4);
                return false;
            }
            subModels.emplace(name, *modelObj);
            if (model["output_movable"]) {
                subModels.find(name)->second.outputDataMovable = model["output_movable"].as<bool>();
            }
        }
        auto load = [&config](TransformInfo &tar, const std::string &name, auto check) {
            for (auto &&rule : config[name.data()]) {
                std::string srcName =
                    rule["name"] ? rule["name"].as<std::string>() : rule["src_name"].as<std::string>();
                std::string dstName =
                    rule["name"] ? rule["name"].as<std::string>() : rule["dst_name"].as<std::string>();
                std::string from = rule["from"] ? rule["from"].as<std::string>() : "root";
                std::string to = rule["to"] ? rule["to"].as<std::string>() : "root";
                check(from, to);
                tar.rules[from][srcName].push_back(TransformInfo::Action{
                    .to = to,
                    .dstName = dstName,
                });
            }
        };

        TransformInfo init;
        load(init, "init_convert", [](auto &from, auto &to) { assert(from == "root"); });
        load(input, "input_convert", [](auto &from, auto &to) { assert(from == "root"); });
        load(output, "output_convert", [](auto &from, auto &to) { assert(from != "root"); });

        std::array<TransformInfo::InputBuffer, 1> buffers{
            TransformInfo::InputBuffer{"root", const_cast<CSValueMap *>(&value), false}};
        auto data = init.transform(buffers);

        for (auto &&[modelName, modelInfo] : subModels) {
            modelInfo.obj->Init(data[modelName]);
        }

        SetState(CSInstanceState::IS_INITIALIZED);
        return true;
    };

    virtual bool Tick(double time) override {
        auto p = profiler.startRecord("tick");

        for (auto &&[_, modelInfo] : subModels) {
            modelInfo.obj->Tick(time);
        }

        return true;
    };

    virtual bool SetInput(const std::unordered_map<std::string, std::any> &value) override {
        auto p1 = profiler.startRecord("before_input");
        std::array<TransformInfo::InputBuffer, 1> buffers{
            TransformInfo::InputBuffer{"root", const_cast<CSValueMap *>(&value), false}};
        auto data = input.transform(buffers);

        auto p2 = profiler.startRecord("input", &p1);
        for (auto &&[modelName, inputData] : data) {
            subModels.find(modelName)->second.obj->SetInput(inputData);
        }

        return true;
    };

    virtual std::unordered_map<std::string, std::any> *GetOutput() override {
        auto p1 = profiler.startRecord("before_output");
        // set ID
        if (!realInited) {
            for (auto &[name, modelInfo] : subModels) {
                modelInfo.obj->SetID(GetID());
                modelInfo.obj->SetLogFun([this, name](auto msg, auto level) {
                    if (level >= 4) {
                        this->WriteLog(std::format("SubModel[{}]Log: {}", name, msg), level);
                    }
                });
            }
            SetState(CSInstanceState::IS_RUNNING);
            realInited = true;
        }

        std::vector<TransformInfo::InputBuffer> buffers;
        buffers.reserve(subModels.size());

        auto p2 = profiler.startRecord("output", &p1);

        for (auto &&[modelName, modelInfo] : subModels) {
            buffers.emplace_back(modelName, modelInfo.obj->GetOutput(), modelInfo.outputDataMovable);
        }

        auto p3 = profiler.startRecord("after_output", &p2);

        auto data = output.transform(buffers);

        if (auto it = data.find("root"); it != data.end()) {
            outputBuffer = std::move(it->second);
            data.erase(it);
        } else {
            outputBuffer.clear();
        }
        outputBuffer.emplace("ForceSideID", GetForceSideID());
        outputBuffer.emplace("ModelID", GetModelID());
        outputBuffer.emplace("InstanceName", GetInstanceName());
        outputBuffer.emplace("ID", GetID());
        outputBuffer.emplace("State", uint16_t(GetState()));

        for (auto &&[modelName, inputData] : data) {
            subModels.find(modelName)->second.obj->SetInput(inputData);
        }

        return &outputBuffer;
    };
    ~MyAssembledModel() {
        if (!profile.empty()) {
            std::ofstream ofs(profile);
            ofs << profiler.getResult() << std::endl;
        }
    }

  private:
    bool realInited = false;

    Profiler profiler{"before_input", "input", "before_output", "output", "after_output", "tick"};
    std::string profile;
    CSValueMap outputBuffer;
    TransformInfo input, output;
    std::unordered_map<std::string, ModelInfo> subModels;
};

extern "C" {
__declspec(dllexport) CSModelObject *CreateModelObject() { return new MyAssembledModel; };
__declspec(dllexport) void DestroyMemory(void *mem, bool is_array) {
    if (is_array) {
        delete[] ((MyAssembledModel *)mem);
    } else {
        delete (MyAssembledModel *)mem;
    }
};
}