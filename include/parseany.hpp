/**
 * @file parseany.hpp
 * @author djw
 * @brief
 * @date 2023-05-11
 *
 * @details
 *
 * @par history
 * <table>
 * <tr><th>Author</th><th>Date</th><th>Changes</th></tr>
 * <tr><td>djw</td><td>2023-05-11</td><td>Initial version.</td></tr>
 * </table>
 */
#pragma once

#include <any>
#include <expected>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstring>

#include "printcsvaluemap.hpp"
#include "rapidxml-1.13/rapidxml.hpp"

namespace tools::myany {

struct XMLFormat {
    inline static constexpr auto ArrayFormat = "<li>{}</li>";
    inline static constexpr auto CSValueMapFormat = "<c>{}</c>";
    inline static constexpr auto PairFormat = "<{0}>{1}</{0}>";
    inline static constexpr auto StringFormat = "<string>{}</string>";
    inline static constexpr auto NumericalFormat = "<{0}>{1}</{0}>";
    inline static constexpr auto UnknownFormat = "<nil/>";
    inline static constexpr auto Spliter = "";
};

std::string toXML(const std::any &v) { return printAnyToString<XMLFormat>(v); }

enum struct parseError {
    different_type_in_same_list,
    unknown_type,
};

std::expected<std::any, parseError> parseXML(rapidxml::xml_node<char> *node) {
    using namespace std;
    using namespace literals;
    if (node->name() == "li"sv) {
        vector<any> ret;
        auto tmp = node->first_node();
        if (!tmp) {
            return std::move(ret);
        }
        string_view type{tmp->name(), tmp->name_size()};
        for (auto p = node->first_node(); p; p = p->next_sibling()) {
            if (p->name() != type) {
                return std::unexpected(parseError::different_type_in_same_list);
            }
            auto tmp = parseXML(p);
            if(!tmp){
                return std::unexpected(tmp.error());
            }
            ret.push_back(std::move(tmp.value()));
        }
        return std::move(ret);
    } else if (node->name() == "c"sv) {
        unordered_map<string, any> ret;
        for (auto p = node->first_node(); p; p = p->next_sibling()) {
            auto tmp = parseXML(p->first_node());
            if(!tmp){
                return std::unexpected(tmp.error());
            }
            ret.emplace(string{p->name(), p->name_size()}, std::move(tmp.value()));
        }
        return std::move(ret);
    } else {
        string value = {node->value(), node->value_size()};
        string_view type{node->name(), node->name_size()};
        if (type == "bool"sv)
            return bool(stoi(value));
        if (type == "int8_t"sv)
            return int8_t(stoi(value));
        if (type == "uint8_t"sv)
            return uint8_t(stoi(value));
        if (type == "int16_t"sv)
            return int16_t(stoi(value));
        if (type == "uint16_t"sv)
            return uint16_t(stoi(value));
        if (type == "int32_t"sv)
            return int32_t(stoi(value));
        if (type == "uint32_t"sv)
            return uint32_t(stoul(value));
        if (type == "int64_t"sv)
            return int64_t(stoll(value));
        if (type == "uint64_t"sv)
            return uint64_t(stoull(value));
        if (type == "float"sv)
            return float(stof(value));
        if (type == "double"sv)
            return double(stof(value));
        if (type == "string"sv)
            return string{value};
        return std::unexpected(parseError::unknown_type);
    }
}

std::expected<std::any, parseError> parseXMLString(const std::string &node) {
    size_t strLen = node.size() + 1;
    auto buffer = std::make_unique<char[]>(strLen);
    memcpy(buffer.get(), node.c_str(), strLen);
    rapidxml::xml_document<> doc;
    doc.parse<rapidxml::parse_default>(buffer.get());
    auto root = doc.first_node();
    return parseXML(root);
}

} // namespace tools::myany

template <>
struct std::formatter<tools::myany::parseError, char> {
    template <class ParseContext> constexpr ParseContext::iterator parse(ParseContext &ctx) { return ctx.begin(); }

    template <class FmtContext> FmtContext::iterator format(tools::myany::parseError err, FmtContext &ctx) const {
        std::ostringstream out;
        if (err == tools::myany::parseError::different_type_in_same_list)
            out << "[different_type_in_same_list]";
        else
            out << "[unknown_type]";
        return std::ranges::copy(std::move(out).str(), ctx.out()).out;
    }
};
