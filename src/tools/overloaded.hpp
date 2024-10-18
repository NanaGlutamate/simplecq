#pragma once

#ifndef __SRC_TOOLS_OVERLOADED_HPP__
#define __SRC_TOOLS_OVERLOADED_HPP__

template <typename... T>
struct overloaded : public T... {
    using T::operator()...;
};
template <typename... T>
overloaded(T...) -> overloaded<T...>;

#endif // __SRC_TOOLS_OVERLOADED_HPP__