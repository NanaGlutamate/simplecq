/**
 * @file modelmanager.hpp
 * @author glutamate
 * @brief
 * @version 0.1
 * @date 2024-05-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>

#include "callback.hpp"
#include "dllop.hpp"

using CSValueMap = std::unordered_map<std::string, std::any>;

struct ModelEntity {
    std::string modelTypeName;
    ModelObjHandle handle;
};

struct ModelManager {
    /**
     * @brief create an initialized model entity, append it to model vector
     *
     * @param ID model id
     * @param sideID model side id
     * @param type model type
     * @param dynamic is dynamically created, should not be false when create in running, or will change model vector
     */
    std::expected<ModelEntity*, std::string> createModel(uint64_t ID, uint16_t sideID, const std::string& type,
                                                          const CSValueMap &value, bool dynamic) {
        return loader.loadModel(type).and_then(
            [&, this](ModelEntity entity) -> std::expected<ModelEntity*, std::string> {
                std::vector<ModelEntity> &tar = dynamic ? dynamicModels : models;
                auto &model = tar.emplace_back(std::move(entity));
                model.handle.obj->SetID(ID);
                model.handle.obj->SetForceSideID(sideID);
                model.handle.obj->SetLogFun([this, type](const std::string &msg, uint32_t level) {
                    callback.writeLog(type, msg, level);
                });
                model.handle.obj->SetCommonCallBack(
                    [this](const std::string &type, const std::unordered_map<std::string, std::any> &param) {
                        callback.commonCallBack(type, param);
                        return std::string{};
                    });
                auto ans = doWithCatch([&] {
                    if (!model.handle.obj->Init(value)) {
                        throw std::logic_error("init function return false");
                    }
                });
                if (!ans) {
                    tar.pop_back();
                    return std::unexpected(ans.error());
                }
                modelTypes.emplace(type);
                return &model;
            });
    }
    std::expected<void, std::string> loadDll(const std::string &name, const std::string &path, bool move) {
        return loader.loadDll(name, path, move);
    }
    void createDynamicModel() {
        for (auto &&[ID, sideID, param, type] : callback.createModelCommands) {
            if (auto res = createModel(ID, sideID, type, param, true); !res) {
                callback.writeLog("Engine", std::format("Exception When Create Dynamic Model: {}", res.error()), 5);
            }
        }
        callback.createModelCommands.clear();
    }

    // TODO: std::unordered_map<std::string, ModelObjHandle>
    std::vector<ModelEntity> models;
    // dynamic created models
    std::vector<ModelEntity> dynamicModels;
    std::set<std::string> modelTypes;

    struct ModelLoader {
        std::expected<ModelEntity, std::string> loadModel(const std::string &type) {
            auto it = dlls.find(type);
            if (it == dlls.end()) {
                return std::unexpected("no such type");
            }
            auto model = ::loadModel(it->second);
            model.outputDataMovable = movable[type];
            return ModelEntity{type, std::move(model)};
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

      private:
        std::unordered_map<std::string, ModelDllInterface> dlls;
        std::unordered_map<std::string, bool> movable;
    } loader;
    inline static CallbackHandler callback = {};
};