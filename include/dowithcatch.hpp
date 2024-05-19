#pragma once

#include <concepts>
#include <expected>
#include <string>

template <std::invocable<> Func>
inline std::expected<std::invoke_result_t<Func>, std::string> doWithCatch(Func &&fun) noexcept {
    try {
        if constexpr (!std::is_same_v<void, std::invoke_result_t<Func>>) {
            return fun();
        } else {
            fun();
            return {};
        }
    } catch (std::exception &err) {
        return std::unexpected(err.what());
    } catch (...) {
        return std::unexpected("[unhandled exception]");
    }
}