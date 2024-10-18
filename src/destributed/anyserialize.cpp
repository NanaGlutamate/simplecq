#include "anyserialize.hpp"

namespace {

void writeBuffer(uint64_t data, std::vector<std::byte>& buffer) {

}

};

namespace tools::myany {

inline bool serialize(
    Context& ctx, 
    uint64_t protoId, 
    const std::unordered_map<std::string, std::any>& data, 
    std::vector<std::byte>& buffer
) /* noexcept */ {
    auto& thisProto = ctx.proto[protoId];
    for (auto&& [k, v] : data) {
        auto it = thisProto.find(k);
        if (it == thisProto.end()) {
            // check for real types, only CSValueMap is not buildin
            // toro: replace err with no throw error handler
            uint64_t newProtoId = visit<err>(overloaded {
                [&](const std::unordered_map<std::string, std::any>&) { return ctx.newUuid(); },
                [&](const auto&) { return static_cast<uint64_t>(ctx.protoIdMap[typeid(v)]); }
            }, v);
            uint64_t newMemberId = thisProto.size();
            // todo: check proto size (member id upper bound)

            it = thisProto.emplace(k, Context::ValueInfo{newProtoId, newMemberId}).first;

            ctx.dirtyProto.insert(protoId);
        }
        auto [memberProtoId, memberId] = it->second;
        if (memberProtoId < Context::BuildInProtoId::kMaxBuildInProtoId) {
            // todo: build in types
        } else {
            writeBuffer(memberId, buffer);
            serialize(
                ctx,
                memberProtoId,
                // todo: mark dirty when nullptr
                *std::any_cast<std::unordered_map<std::string, std::any>>(&v),
                buffer
            );
        }
    }
    return false;
}

}