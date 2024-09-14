#pragma once

#include <expected>
#include <string>

#include "csmodel_base.h"

struct ModelDllInterface {
    using CreateModelFunction = auto (*)() -> CSModelObject *;
    using DestoryModelFunction = auto (*)(void *, bool) -> void;
    CreateModelFunction createFunc;
    DestoryModelFunction destoryFunc;
};

std::expected<ModelDllInterface, std::string_view> loadDll(const std::string &dllPath);

std::string getLibDir();

struct ModelObjHandle {
    ModelDllInterface dll{nullptr, nullptr};
    CSModelObject *obj = nullptr;
    bool outputDataMovable = false;
    ModelObjHandle() = default;
    ModelObjHandle(const ModelObjHandle &) = delete;
    void createAs(ModelObjHandle& o) const {
        o.dll = dll;
        o.obj = dll.createFunc();
        o.outputDataMovable = outputDataMovable;
    }
    ModelObjHandle(ModelObjHandle &&o) noexcept {
        obj = o.obj;
        dll = o.dll;
        outputDataMovable = o.outputDataMovable;
        o.obj = nullptr;
    }
    ModelObjHandle& operator=(ModelObjHandle&& o) noexcept {
        if (obj) {
            dll.destoryFunc(obj);
        }
        obj = o.obj;
        dll = o.dll;
        outputDataMovable = o.outputDataMovable;
        o.obj = nullptr;
    }
    ~ModelObjHandle() {
        if (obj) {
            dll.destoryFunc(obj, false);
        }
    }
};

inline std::expected<ModelObjHandle, std::string_view> loadModel(const std::string &dllName) {
    return loadDll(dllName).and_then([](ModelDllInterface dll) -> std::expected<ModelObjHandle, std::string_view> {
        ModelObjHandle info;
        info.dll = dll;
        info.obj = dll.createFunc();
        return info;
    });
}

inline ModelObjHandle loadModel(ModelDllInterface dll) {
    ModelObjHandle info;
    info.dll = dll;
    info.obj = dll.createFunc();
    return info;
}