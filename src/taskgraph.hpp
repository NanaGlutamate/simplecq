// #pragma once

// #include <taskflow/taskflow.hpp>
// #include <optional>
// #include <tuple>

// template <typename Ty>
// inline constexpr bool is_task_node_v;

// template <typename Ty>
// concept is_task_node = is_task_node_v<Ty>;

// struct context {
//     template <typename Ret, typename ...Ty>
//         requires (is_task_node<Ty> && ...)
//     struct task_node {
//         tf::Task task;
//         std::optional<Ret, std::tuple<Ty...>> data;
//     };

//     template <typename Func, typename ...Val>
//     auto taked_invoke(Func&& f, Val&& ...val) {

//     }
// };

// template <typename Ty>
// inline constexpr bool is_task_node_v<Ty> = false;

// template <typename Ret, typename ...Ty>
// inline constexpr bool is_task_node_v<context::task_node<Ret, Ty...>> = true;