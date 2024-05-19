#pragma once

#include <concepts>
#include <type_traits>
#include <any>

struct DenseObjectLayout {
    size_t size;
};

template <typename Ty>
    requires std::is_trivially_copyable_v<Ty> &&
        std::is_base_of_v<Dense<Ty>, Ty> &&
        std::is_convertible_v<Ty*, Dense<Ty>*>
struct Dense {
    constexpr static size_t obj_size = sizeof(Ty);
    static_assert(obj_size == sizeof(Dense<Ty>));
    DenseObjectLayout get
};