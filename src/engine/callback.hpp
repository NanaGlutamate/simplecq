/**
 * @file callback.hpp
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
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "parseany.hpp"

using CSValueMap = std::unordered_map<std::string, std::any>;

struct CallbackHandler {
    size_t log_level = 0;
    bool enable_log = true;
    std::ofstream log_file = {};
    // std::string curr_log_file = "None";
    std::mutex callback_lock;

    struct CreateModelCommand {
        uint64_t ID;
        uint16_t sideID;
        CSValueMap param;
        std::string type;
    };
    std::vector<CreateModelCommand> createModelCommands;

    void writeLog(std::string_view src, std::string_view msg, int32_t level) noexcept {
        if (level < log_level || !enable_log || !log_file)
            return;
        std::unique_lock lock{callback_lock};
        log_file << std::format("[{}-{}]: {}\n", src, level, msg);
    }

    std::string commonCallBack(const std::string &type, const std::unordered_map<std::string, std::any> &param) {
        // dynamic create entity
        using namespace std::literals;
        if (type == "CreateEntity"sv) {
            try {
                uint64_t ID = std::any_cast<uint64_t>(param.find("ID")->second);
                uint16_t sideID = std::any_cast<uint16_t>(param.find("ForceSideID")->second);
                std::string type = std::any_cast<std::string>(param.find("ModelID")->second);
                std::unique_lock lock{callback_lock};
                createModelCommands.push_back({ID, sideID, param, std::move(type)});
            } catch (std::bad_any_cast &) {
                writeLog("Engine",
                         std::format("Data Type Mismatch while dynamic create entity: {}({})", type,
                                     tools::myany::printCSValueMapToString(param)),
                         5);
            }
        } else {
            writeLog("Engine",
                     std::format("Unsupport Callback function call: {}({})", type,
                                 tools::myany::printCSValueMapToString(param)),
                     5);
        }
        return "";
    }
};