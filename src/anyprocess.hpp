/**
 * @file anyprocess.hpp
 * @author djw
 * @brief
 * @date 2023-04-24
 *
 * @details
 *
 * @par history
 * <table>
 * <tr><th>Author</th><th>Date</th><th>Changes</th></tr>
 * <tr><td>djw</td><td>2023-04-24</td><td>Initial version.</td></tr>
 * </table>
 */
#pragma once

#include <any>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace tools::myany {

/**
 * @brief simple functor to call when meet unknown type
 *
 */
struct err {
    struct GernalType {
        template <typename Ty> operator Ty() const { return Ty{}; }
    };
    [[noreturn]] GernalType operator()(const std::any &v) const {
        throw std::logic_error(std::string("Unknown held type of any when visit: ") + v.type().name());
    }
};

template <typename Func, typename Any>
concept invocable_on_legal_type = requires(Func f, Any v) {
    f(*std::any_cast<double>(&v));
    f(*std::any_cast<float>(&v));
    f(*std::any_cast<std::unordered_map<std::string, std::any>>(&v));
    f(*std::any_cast<std::vector<std::any>>(&v));
    f(*std::any_cast<std::string>(&v));
    f(*std::any_cast<int64_t>(&v));
    f(*std::any_cast<uint64_t>(&v));
    f(*std::any_cast<int32_t>(&v));
    f(*std::any_cast<uint32_t>(&v));
    f(*std::any_cast<int16_t>(&v));
    f(*std::any_cast<uint16_t>(&v));
    f(*std::any_cast<int8_t>(&v));
    f(*std::any_cast<uint8_t>(&v));
    f(*std::any_cast<bool>(&v));
};

/**
 * @brief use function to visit data in any
 *
 * @tparam FuncOnErr functor to call when meet unknown type
 * @param func function to visit data
 * @param v target std::any
 * @return based on func
 */
template <typename FuncOnErr, typename Any, invocable_on_legal_type<Any> Func>
    requires std::is_same_v<std::any, std::remove_cvref_t<Any>> && std::is_invocable_v<FuncOnErr, Any>
inline auto visit(Func &&func, Any &&v) {
    // TODO: fix behaviour when Any = std::any&&, should call func(std::vector<std::any>&&) | func(CSValueMap&&) |
    // func(std::string&&)

    // TODO: use if constexpr to identify if func can called on data in std::any
    if (v.type() == typeid(double)) {
        return func(*std::any_cast<double>(&v));
    } else if (v.type() == typeid(float)) {
        return func(*std::any_cast<float>(&v));
    } else if (v.type() == typeid(std::unordered_map<std::string, std::any>)) {
        return func(*std::any_cast<std::unordered_map<std::string, std::any>>(&v));
    } else if (v.type() == typeid(std::vector<std::any>)) {
        return func(*std::any_cast<std::vector<std::any>>(&v));
    } else if (v.type() == typeid(std::string)) {
        return func(*std::any_cast<std::string>(&v));
    } else if (v.type() == typeid(int64_t)) {
        return func(*std::any_cast<int64_t>(&v));
    } else if (v.type() == typeid(uint64_t)) {
        return func(*std::any_cast<uint64_t>(&v));
    } else if (v.type() == typeid(int32_t)) {
        return func(*std::any_cast<int32_t>(&v));
    } else if (v.type() == typeid(uint32_t)) {
        return func(*std::any_cast<uint32_t>(&v));
    } else if (v.type() == typeid(bool)) {
        return func(*std::any_cast<bool>(&v));
    } else if (v.type() == typeid(int8_t)) {
        return func(*std::any_cast<int8_t>(&v));
    } else if (v.type() == typeid(uint8_t)) {
        return func(*std::any_cast<uint8_t>(&v));
    } else if (v.type() == typeid(int16_t)) {
        return func(*std::any_cast<int16_t>(&v));
    } else if (v.type() == typeid(uint16_t)) {
        return func(*std::any_cast<uint16_t>(&v));
    } else {
        if constexpr (std::is_constructible_v<decltype(func(*std::any_cast<double>(&v))),
                                              decltype(std::invoke(FuncOnErr{}, v))>) {
            return static_cast<decltype(func(*std::any_cast<double>(&v)))>(
                std::invoke(FuncOnErr{}, std::forward<Any>(v)));
        } else {
            std::invoke(FuncOnErr{}, std::forward<Any>(v));
            err{}("unhandled failure in myany::visit");
        }
    }
}

/**
 * @brief check if data in two std::any is equal
 *
 * @attention func fit invocable_on_legal_type must allowed to be called on data in input std::any
 *
 * @param lhs first std::any
 * @param rhs second std::any
 * @return bool
 */
inline bool anyEqual(const std::any &lhs, const std::any &rhs) {
    if (lhs.type() != rhs.type()) {
        return false;
    }
    return myany::visit<myany::err>(
        [&](auto &l) -> bool {
            using namespace std;
            auto realRhsPtr = any_cast<remove_cvref_t<decltype(l)>>(&rhs);
            if constexpr (is_same_v<vector<any>, remove_cvref_t<decltype(l)>>) {
                if (l.size() != realRhsPtr->size()) {
                    return false;
                }
                for (size_t i = 0; i < l.size(); i++) {
                    if (!anyEqual(l[i], (*realRhsPtr)[i])) {
                        return false;
                    }
                }
                return true;
            } else if constexpr (is_same_v<unordered_map<string, any>, remove_cvref_t<decltype(l)>>) {
                if (l.size() != realRhsPtr->size()) {
                    return false;
                }
                for (auto &&[name, v] : l) {
                    if (auto it = realRhsPtr->find(name); it == realRhsPtr->end() || !anyEqual(v, it->second)) {
                        return false;
                    }
                }
                return true;
            } else {
                return l == *realRhsPtr;
            }
        },
        lhs);
}

} // namespace tools::myany