#pragma once

#include <string>
#include <unordered_map>
#include <any>
#include <span>
#include <cassert>

struct TransformInfo {
    struct Action {
        std::string to, dstName;
    };
    struct InputBuffer {
        std::string name;
        std::unordered_map<std::string, std::any> *buffer;
        bool movable;
    };
    // from, srcname, action
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Action>>> rules;
    template <size_t N>
    std::unordered_map<std::string, std::unordered_map<std::string, std::any>> transform(std::span<InputBuffer, N> buffers) {
        std::unordered_map<std::string, std::unordered_map<std::string, std::any>> ret;
        for (auto &&[bufferName, data, movable] : buffers) {
            if (auto it = rules.find(bufferName); it != rules.end()) {
                for (auto &&[name, value] : *data) {
                    if (auto it2 = it->second.find(name); it2 != it->second.end()) {
                        auto &actions = it2->second;
                        assert(actions.size());
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