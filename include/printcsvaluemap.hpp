/**
 * @file printcsvaluemap.hpp
 * @author djw
 * @brief Tools/CSValueMap printer
 * @date 2023-03-28
 *
 * @details Includes a tool function to print CSValueMap.
 *
 * @par history
 * <table>
 * <tr><th>Author</th><th>Date</th><th>Changes</th></tr>
 * <tr><td>djw</td><td>2023-03-28</td><td>Initial version.</td></tr>
 * </table>
 */
#pragma once

#include <any>
#include <format>
#include <iostream>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <array>
#include <string_view>

#include "tools/anyprocess.hpp"
#include "tools/stringprocess.hpp"

namespace tools::myany {

namespace helper {

    template <typename Test, typename ...List>
    struct get_type_list_index;

    template <typename Test, typename ListStart, typename ...List>
    struct get_type_list_index<Test, ListStart, List...> {
        constexpr static inline std::size_t value = get_type_list_index<Test, List...>::value + 1;
    };

    template <typename Test, typename ...List>
    struct get_type_list_index<Test, Test, List...> {
        constexpr static inline std::size_t value = 0;
    };

    template <typename... Ty>
    struct NameTable {
        std::array<std::string_view, sizeof...(Ty)> table;
        template <typename T>
        constexpr std::string_view getName() const {
            return table[get_type_list_index<T, Ty...>::value];
        }
    };

    inline constexpr NameTable<bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double> numericalTypeNameTable{
        std::array<std::string_view, 11>{"bool", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double"}
    };

}

struct DefaultFormat {
    inline static constexpr auto ArrayFormat = "[{}]";
    inline static constexpr auto CSValueMapFormat = "{{{}}}";
    inline static constexpr auto PairFormat = "\"{}\" : {}";
    inline static constexpr auto StringFormat = "\"{}\"";
    inline static constexpr auto NumericalFormat = "{1}";
    inline static constexpr auto UnknownFormat = "[[Unknown]]";
    inline static constexpr auto Spliter = ", ";
};

struct PythonFormat {
    inline static constexpr auto ArrayFormat = "[{}]";
    inline static constexpr auto CSValueMapFormat = "{{{}}}";
    inline static constexpr auto PairFormat = "\"{}\" : {}";
    inline static constexpr auto StringFormat = "\"{}\"";
    inline static constexpr auto NumericalFormat = "{1}";
    inline static constexpr auto UnknownFormat = "[[Unknown]]";
    inline static constexpr auto Spliter = ", ";
};

struct CppFormat {
    inline static constexpr auto ArrayFormat = "std::vector<std::any>{{{}}}";
    inline static constexpr auto CSValueMapFormat = "CSValueMap{{{}}}";
    inline static constexpr auto PairFormat = "{{std::string(R\"({})\"), {}}}";
    inline static constexpr auto StringFormat = "std::string(R\"({})\")";
    inline static constexpr auto NumericalFormat = "{}({})";
    inline static constexpr auto UnknownFormat = "void(0)";
    inline static constexpr auto Spliter = ", ";
};

template <typename Formation = DefaultFormat>
std::string printCSValueMapToString(const std::unordered_map<std::string, std::any> &v);

template <typename Formation = DefaultFormat>
inline std::string printAnyToString(const std::any &a) {
    struct UnknownTypeHandler{
        std::string operator()(const std::any &v){
            return Formation::UnknownFormat;
        }
    };
    return myany::visit<UnknownTypeHandler>(
        [](const auto& a) {
            if constexpr (std::is_same_v<std::vector<std::any>, std::remove_cvref_t<decltype(a)>>) {
                return std::format(Formation::ArrayFormat, mystr::join(a | std::views::transform(printAnyToString<Formation>), Formation::Spliter));
            }
            else if constexpr (std::is_same_v<std::unordered_map<std::string, std::any>,
                std::remove_cvref_t<decltype(a)>>) {
                return printCSValueMapToString<Formation>(a);
            }
            else if constexpr (std::is_same_v<std::string, std::remove_cvref_t<decltype(a)>>) {
                return std::format(Formation::StringFormat, a);
            }
            else {
                return std::format(Formation::NumericalFormat, helper::numericalTypeNameTable.getName<std::remove_cvref_t<decltype(a)>>(), std::to_string(a));
            }
        },
        a);
}

template <typename Formation>
inline std::string printCSValueMapToString(const std::unordered_map<std::string, std::any> &v) {
    using namespace std;
    return std::format(Formation::CSValueMapFormat, mystr::join(v | std::views::transform([](const auto& p) {
        return std::format(Formation::PairFormat, p.first, printAnyToString<Formation>(p.second));
        }),
        Formation::Spliter));
}

/**
 * @brief tool function to print CSValueMap to std::cout
 *
 * @param v CSValuMap need to be printed
 */
inline void printCSValueMap(const std::unordered_map<std::string, std::any> &v) {
    std::cout << printCSValueMapToString(v) << std::endl;
}

} // namespace rulejit