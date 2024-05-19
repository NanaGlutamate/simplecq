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
        // TODO: string_view
        std::string name;
        std::unordered_map<std::string, std::any> *buffer;
        bool movable;
    };
    // from, srcname, action
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Action>>> rules;
    template <size_t N>
    [[gnu::hot]] std::unordered_map<std::string, std::unordered_map<std::string, std::any>> transform(std::span<InputBuffer, N> buffers) const {
        std::unordered_map<std::string, std::unordered_map<std::string, std::any>> ret;
        transform(ret, buffers);
        return ret;
    }
    template <size_t N>
    [[gnu::hot]] void transform(std::unordered_map<std::string, std::unordered_map<std::string, std::any>>& ret, std::span<InputBuffer, N> buffers) const {
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
    }
};