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
#include "datatransform.hpp"
#include "dllop.hpp"
#include "profile.hpp"

namespace {

using namespace std::literals;

using CSValueMap = std::unordered_map<std::string, std::any>;

} // namespace

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
        auto p = profiler.startRecord("root: init");

        if (!log_) {
            log_ = [](auto msg, auto) { std::cout << std::format("[AssembleModel]: ", msg) << std::endl; };
        }

        auto location = getLibDir();
        // std::ifstream configFile(location + "assemble.yml");
        auto configFile = location + "assemble.yml";
        auto config = YAML::LoadFile(configFile);
        // fkyaml::node config = fkyaml::node::deserialize(configFile);
        for (auto &&model : config["models"]) {
            auto name = model["model_name"].as<std::string>();
            auto dllName = model["dll_name"].as<std::string>();
            if (dllName.starts_with("./") || (!dllName.contains('/') && !dllName.contains('\\'))) {
                dllName = location + dllName;
            }
            auto modelObj = loadModel(dllName);
            if (!modelObj.has_value()) {
                auto errorInfo = std::format("[AssembledModel] error when loading [{}]: ", name);
                errorInfo += modelObj.error();
                WriteLog(errorInfo, 4);
                return false;
            }
            auto [it, _] = subModels.emplace(name, std::move(*modelObj));
            if (model["output_movable"]) {
                it->second.outputDataMovable = model["output_movable"].as<bool>();
            }
        }
        if (&value != &initValue) {
            // first init
            if (config["config"]) {
                auto n = config["config"];
                if (n["profile"]) {
                    profileFile = n["profile"].as<std::string>();
                }
                if (n["restart_key"]) {
                    restartKey = n["restart_key"].as<std::string>();
                    // if(&value != &initValue)
                    initValue = value;
                }
                if (n["side_filter"]) {
                    sideFilter = n["side_filter"].as<std::string>();
                }
                if (n["log_level"]) {
                    logLevel = n["log_level"].as<uint32_t>();
                }
            }
            auto load = [&config](TransformInfo &tar, const std::string &name, auto check) {
                for (auto &&rule : config[name.data()]) {
                    std::string from = rule["from"] ? rule["from"].as<std::string>() : "root";
                    std::string to = rule["to"] ? rule["to"].as<std::string>() : "root";
                    check(from, to);
                    for (auto &&value : rule["values"]) {
                        std::string srcName =
                            value["name"] ? value["name"].as<std::string>() : value["src_name"].as<std::string>();
                        std::string dstName =
                            value["name"] ? value["name"].as<std::string>() : value["dst_name"].as<std::string>();
                        tar.rules[from][srcName].push_back(TransformInfo::Action{
                            .to = to,
                            .dstName = dstName,
                        });
                    }
                }
            };
            load(init, "init_convert", [](auto &from, auto &to) { assert(from == "root"); });
            load(input, "input_convert", [](auto &from, auto &to) { assert(from == "root"); });
            load(output, "output_convert", [](auto &from, auto &to) { assert(from != "root"); });
        }
        
        SetState(CSInstanceState::IS_INITIALIZED);

        std::array<TransformInfo::InputBuffer, 1> buffers{
            TransformInfo::InputBuffer{"root", const_cast<CSValueMap *>(&value), false}};
        auto data = init.transform(std::span{buffers});

        p.end();

        for (auto &&[modelName, modelInfo] : subModels) {
            auto p = profiler.startRecord(modelName + ": init");
            modelInfo.obj->SetLogFun([this, modelName](auto msg, auto level) {
                if (level >= logLevel) {
                    WriteLog(std::format("SubModel[{}]Log: {}", modelName, msg), level);
                }
            });
            modelInfo.obj->SetID(GetID());
            modelInfo.obj->SetForceSideID(GetForceSideID());

            modelInfo.obj->Init(data[modelName]);
        }

        return true;
    };

    virtual bool Tick(double time) override {
        if (restartFlag) {
            return true;
        }

        for (auto &&[modelName, modelInfo] : subModels) {
            auto p = profiler.startRecord(modelName + ": tick");
            modelInfo.obj->Tick(time);
        }

        return true;
    };

    virtual bool SetInput(const std::unordered_map<std::string, std::any> &value) override {
        if (restartFlag || (restartKey.size() && value.contains(restartKey))) {
            restartFlag = true;
            return true;
        }
        if (!sideFilter.empty()) {
            if (auto it = value.find(sideFilter);
                it != value.end() && GetForceSideID() != std::any_cast<uint16_t>(it->second)) {
                return true;
            }
        }

        auto p1 = profiler.startRecord("root: before_input");
        std::array<TransformInfo::InputBuffer, 1> buffers{
            TransformInfo::InputBuffer{"root", const_cast<CSValueMap *>(&value), false}};
        auto data = input.transform(std::span{buffers});

        p1.end();
        for (auto &&[modelName, inputData] : data) {
            auto p2 = profiler.startRecord(modelName + ": input");
            subModels.find(modelName)->second.obj->SetInput(inputData);
        }

        return true;
    };

    virtual std::unordered_map<std::string, std::any> *GetOutput() override {
        if (restartFlag) {
            restart();
        }

        auto p1 = profiler.startRecord("root: before_output");
        // set ID
        if (!realInited) {
            for (auto &[name, modelInfo] : subModels) {
                modelInfo.obj->SetID(GetID());
                modelInfo.obj->SetForceSideID(GetForceSideID());
            }
            SetState(CSInstanceState::IS_RUNNING);
            realInited = true;
        }

        std::vector<TransformInfo::InputBuffer> buffers;
        buffers.reserve(subModels.size());

        p1.end();

        for (auto &&[modelName, modelInfo] : subModels) {
            auto p2 = profiler.startRecord(modelName + ": output");
            buffers.emplace_back(modelName, modelInfo.obj->GetOutput(), modelInfo.outputDataMovable);
        }

        auto p3 = profiler.startRecord("root: after_output");

        auto data = output.transform(std::span{buffers});

        if (auto it = data.find("root"); it != data.end()) {
            outputBuffer = std::move(it->second);
            if (auto it = outputBuffer.find("State"); it != outputBuffer.end()) {
                SetState(std::any_cast<uint16_t>(it->second));
            }
            data.erase(it);
        } else {
            outputBuffer.clear();
        }
        outputBuffer.emplace("ForceSideID", GetForceSideID());
        outputBuffer.emplace("ModelID", GetModelID());
        outputBuffer.emplace("InstanceName", GetInstanceName());
        outputBuffer.emplace("ID", GetID());
        outputBuffer.emplace("State", uint16_t(GetState()));

        p3.end();

        for (auto &&[modelName, inputData] : data) {
            auto p2 = profiler.startRecord(modelName + ": input");
            subModels.find(modelName)->second.obj->SetInput(inputData);
        }

        return &outputBuffer;
    };
    ~MyAssembledModel() {
        if (!profileFile.empty()) {
            std::ofstream ofs(profileFile);
            ofs << profiler.getResult() << std::endl;
        }
    }

  private:
    bool realInited = false;
    void restart() {
        restartFlag = false;

        subModels.clear();

        Init(initValue);
    }

    Profiler profiler;
    uint32_t logLevel;
    std::string profileFile, restartKey, sideFilter;
    bool restartFlag = false;
    CSValueMap initValue;
    CSValueMap outputBuffer;
    TransformInfo init, input, output;
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