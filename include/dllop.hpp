#pragma once

#include <string>
#include <expected>

#ifdef _WIN32
#include <Windows.h>
#include <atlbase.h>
#include <atlwin.h>
#else
#include <dlfcn.h>
#endif

#include "csmodel_base.h"

struct ModelDllInterface {
    using CreateModelFunction = auto (*)() -> CSModelObject *;
    using DestoryModelFunction = auto (*)(void *, bool) -> void;
    CreateModelFunction createFunc;
    DestoryModelFunction destoryFunc;
};

inline std::expected<ModelDllInterface, std::string_view> loadDll(const std::string &dllPath) {
    ModelDllInterface mi;
#ifdef _WIN32
    auto hmodule = LoadLibraryExA(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else  // _WIN32
    void *hmodule = dlopen(lib_path_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#endif // _WIN32
    if (!hmodule) {
        return std::unexpected("load dll error");
    }
#ifdef _WIN32
    mi.createFunc = (auto (*)()->CSModelObject *)GetProcAddress(hmodule, "CreateModelObject");
    mi.destoryFunc = (auto (*)(void *, bool)->void)GetProcAddress(hmodule, "DestroyMemory");
#else  // _WIN32
    mi.createFunc = (auto (*)()->CSModelObject *)dlsym(hmodule, "CreateModelObject");
    mi.destoryFunc = (auto (*)(void *, bool)->void)dlsym(hmodule, "DestroyMemory");
#endif // _WIN32
    if (!mi.createFunc || !mi.destoryFunc) {
        return std::unexpected("load function error");
    }
    return mi;
}

inline std::string getLibDir() {
    std::string library_dir_;
#ifdef _WIN32
    HMODULE module_instance = _AtlBaseModule.GetModuleInstance();
    char dll_path[MAX_PATH] = {0};
    GetModuleFileNameA(module_instance, dll_path, _countof(dll_path));
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    _splitpath_s(dll_path, drive, dir, fname, ext);
    library_dir_ = drive + std::string(dir) + "\\";
#else
    Dl_info dl_info;
    CSModelObject *(*p)() = &CreateModelObject;
    if (0 != dladdr((void *)(p), &dl_info)) {
        library_dir_ = std::string(dl_info.dli_fname);
        library_dir_ = library_dir_.substr(0, library_dir_.find_last_of('/'));
        library_dir_ += "/";
    }
#endif
    return library_dir_;
}