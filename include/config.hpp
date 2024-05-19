/**
 * @file config.hpp
 * @author glutamate
 * @brief
 * @version 0.1
 * @date 2024-05-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <algorithm>
#include <format>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct Config {
    void syncWithFile(const std::string &filePath) {
        data.clear();
        if (auto ifs = std::ifstream(filePath); ifs) {
            std::string lineBuffer;
            while (!ifs.eof()) {
                std::getline(ifs, lineBuffer);
                if (lineBuffer.empty()) {
                    continue;
                }
                std::string_view line{lineBuffer};
                auto p = line.find_first_of('=');
                auto key = line.substr(0, p);
                auto value = line.substr(p + 1);
                key = key.substr(key.find_first_not_of(' '));
                key = key.substr(0, key.find_last_not_of(' ') + 1);
                value = value.substr(key.find_first_not_of(' '));
                value = value.substr(0, key.find_last_not_of(' ') + 1);
                setValueWithoutWriteFile(std::string(key), std::string(value));
            }
        }
        f = std::ofstream{filePath};
    }

    void listen(const std::string &key, std::function<void(const std::string &)> f) {
        if (auto it = data.find(key); it != data.end()) {
            f(it->second);
        }
        callbacks[key].push_back(std::move(f));
    }

    void setValue(const std::string &key, std::string value) {
        data[key] = std::move(value);
        writeFile();
        auto it = callbacks.find(key);
        if (it == callbacks.end()) {
            return;
        }
        std::ranges::for_each(it->second, [&v{data[key]}](auto &fun) { fun(v); });
    }

    bool isKeyListened(const std::string &key) { return callbacks.contains(key); }

    const std::string &getValue(const std::string &key) {
        auto it = data.find(key);
        if (it == data.end()) {
            static std::string fallback = "[unspecified]";
            return fallback;
        }
        return it->second;
    }

    const std::unordered_map<std::string, std::vector<std::function<void(const std::string &)>>> &getCallBacks() {
        return callbacks;
    }

  private:
    void setValueWithoutWriteFile(const std::string &key, std::string value) {
        data[key] = std::move(value);
        // writeFile();
        auto it = callbacks.find(key);
        if (it == callbacks.end()) {
            return;
        }
        std::ranges::for_each(it->second, [&v{data[key]}](auto &fun) { fun(v); });
    }

    void writeFile() {
        if (f) {
            f.seekp(0);
            for (auto &&[k, v] : data) {
                f << std::format("{}={}\n", k, v);
            }
            f.flush();
        }
    }

    std::ofstream f;
    std::unordered_map<std::string, std::string> data;
    std::unordered_map<std::string, std::vector<std::function<void(const std::string &)>>> callbacks;
};