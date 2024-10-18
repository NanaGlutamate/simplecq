#pragma once

#include <any>
#include <cassert>
#include <span>
#include <string>
#include <unordered_map>

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
    std::unordered_map<std::string, std::unordered_map<std::string, std::any>>
    transform(std::span<InputBuffer, N> buffers) const {
        std::unordered_map<std::string, std::unordered_map<std::string, std::any>> ret;
        transform(ret, buffers);
        return ret;
    }

    template <size_t N>
    void transform(std::unordered_map<std::string, std::unordered_map<std::string, std::any>> &ret,
                   std::span<InputBuffer, N> buffers) const {
        transformWithCallback(
            [&]<typename Ty>(const std::string &a, const std::string &b, Ty &&c) { ret[a][b] = std::forward<Ty>(c); },
            buffers);
    }

    template <std::invocable<const std::string &, const std::string &, std::any &> Func, size_t N>
    void transformWithCallback(Func&& callback, std::span<InputBuffer, N> buffers) const {
        for (auto &&[bufferName, data, movable] : buffers) {
            auto it = rules.find(bufferName);
            if (it == rules.end()) {
                continue;
            }
            for (auto &&[name, value] : *data) {
                auto it2 = it->second.find(name);
                if (it2 == it->second.end()) {
                    continue;
                }
                auto &actions = it2->second;
                assert(actions.size());
                auto size = actions.size();
                if (movable) {
                    for (size_t i = 0; i < size - 1; ++i) {
                        callback(actions[i].to, actions[i].dstName, value);
                    }
                    callback(actions[size - 1].to, actions[size - 1].dstName, std::move(value));
                } else {
                    for (size_t i = 0; i < size; ++i) {
                        callback(actions[i].to, actions[i].dstName, value);
                    }
                }
            }
        }
    }
};