/**
 * @file console.hpp
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
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>

#include "yaml-cpp/yaml.h"

#include "engine/executionengine.hpp"
#include "config.hpp"

struct Scene {
    double x_lower = 0., x_upper = 0., y_lower = 0., y_upper = 0.;
    struct EntityState {
        bool destroied;
        uint16_t ForceSideID;
        double longitude, latitude;
    };
    std::vector<EntityState> buffer;
    void addEntity(uint16_t state, uint16_t ForceSideID, double longitude, double latitude) {
        static auto mymin = [](auto a, auto b) { return a < b ? a : b; };
        static auto mymax = [](auto a, auto b) { return a > b ? a : b; };
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
            for (auto &&[d, f, lon, lat] : buffer | std::views::filter([l, y_diff, y_upper{this->y_upper}](auto &ele) {
                                               auto lat = ele.latitude;
                                               return (lat < y_upper - y_diff * l) &&
                                                      (lat >= y_upper - y_diff * (l + 1));
                                           })) {
                base[size_t(floor((lon - x_lower) / x_diff))] = (d ? 'x' : (f == 1 ? '*' : 'o'));
            }
            std::cout << "|" << base << "|\n";
        }
        std::cout << tools::mystr::repeat("-", row + 2) << "\n";
        buffer.clear();
    }
};

struct ConsoleApp {
    struct Command {
        size_t args;
        std::function<std::expected<void, std::string>(ConsoleApp &, const std::vector<std::string_view> &)> callback;
    };
    static std::map<std::string, Command, std::less<>> commandCallbacks;
    Config cfg;

    void initCfg();

    std::expected<void, std::string> processCommand(std::string_view command) {
        auto line = std::views::split(command, ' ') |
                    std::views::transform([](auto &&x) { return std::string_view{x}; }) |
                    std::views::filter([](std::string_view sv) { return sv != ""; }) | std::ranges::to<std::vector>();
        using namespace std::literals;

        if (line.empty()) {
            return {};
        } else {
            auto it = commandCallbacks.find(line[0]);
            if (it == commandCallbacks.end()) {
                std::string s;
                for (auto &&[k, v] : commandCallbacks) {
                    s += std::format("{}, ", k);
                }
                return std::unexpected(std::format("unknown command, all commands: {}", s));
            }
            if (it->second.args != line.size() - 1) {
                return std::unexpected("argument count mismatch");
            }
            return it->second.callback(*this, line);
        }
    }

    void replMode() {
        std::string command;
        while (std::cin && !std::cin.eof()) {
            std::cout << ">>> ";
            std::getline(std::cin, command);
            processCommand(command).or_else([](const std::string &err) -> std::expected<void, std::string> {
                std::cout << err << std::endl;
                return {};
            });
        }
    }

    void printBuffer() {
        for (auto &&[tar, l] : engine.tm.buffer.topic_buffer) {
            std::cout << tar << ":" << std::endl;
            for (auto &&cs : l) {
                std::cout << "\t";
                tools::myany::printCSValueMap(cs);
            }
        }
    }

    Scene s;
    void draw() {
        if (auto it = engine.tm.buffer.topic_buffer.find("root"); it != engine.tm.buffer.topic_buffer.end()) {
            for (auto &&lonlat : it->second) {
                s.addEntity(std::any_cast<uint16_t>(lonlat.find("State")->second),
                            std::any_cast<uint16_t>(lonlat.find("ForceSideID")->second),
                            std::any_cast<double>(lonlat.find("longitude")->second),
                            std::any_cast<double>(lonlat.find("latitude")->second));
            }
            std::cout << std::format("fps: {} rate: {}\n\n", engine.s.fps, engine.s.fps * engine.s.dt / 1000);
            s.draw(12);
        }
    }

    std::expected<void, std::string> loadFile(const std::string &config_file) {
        engine.clear();
        auto config = YAML::LoadFile(config_file);
        for (auto &&n : config["model_types"]) {
            // TODO: composed scene with relative file path
            auto succ = engine.mm.loadDll(n["model_type_name"].as<std::string>(), n["dll_path"].as<std::string>(),
                                          n["output_movable"].as<bool>(false));
            if (!succ) {
                return std::unexpected(succ.error());
            }
        }
        for (auto &&n : config["models"]) {
            auto type = n["model_type"].as<std::string>();
            auto id = n["id"].as<uint64_t>();
            auto sideID = n["side_id"].as<uint16_t>();
            auto ans = tools::myany::parseXMLString(n["init_value"].as<std::string>());
            if (!ans) {
                return std::unexpected(
                    std::format("error when parse \"{}\" : {}", n["init_value"].as<std::string>(), ans.error()));
            }
            if (ans.value().type() != typeid(CSValueMap)) {
                return std::unexpected(
                    std::format("error when parse \"{}\" : must be CSValueMap", n["init_value"].as<std::string>()));
            }
            auto value = std::any_cast<CSValueMap>(std::move(ans.value()));
            value.emplace("ForceSideID", sideID);
            value.emplace("ID", id);
            if (auto ans = engine.mm.createModel(id, sideID, type, value, false); !ans) {
                return std::unexpected(std::format("Exception When Model[{}]Init: {}", type, ans.error()));
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
            engine.tm.topics[from].push_back(
                TopicManager::TopicInfo{n["members"].as<std::vector<std::string>>(), std::move(trans)});
        }
        engine.buildGraph();
        return std::expected<void, std::string>();
    }

    ExecutionEngine engine;
    size_t draw_rate = 0;
};