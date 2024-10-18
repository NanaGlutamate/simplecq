#pragma once

#include <typeindex>
#ifndef __SRC_DESTRIBUTED_ANYSERIALIZE_HPP__
#define __SRC_DESTRIBUTED_ANYSERIALIZE_HPP__

#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <array>

#include "anyprocess.hpp"
#include "tools/overloaded.hpp"

// struct message {
//     union proto {
//         struct {
//             uint64_t proto_uuid : 31;
//             uint64_t reserved : 1;
//         } proto[];
//         uint64_t recycled_proto_uuid;
//     };
//     char data[];
// };

namespace tools::myany {

struct Context {
    enum BuildInProtoId {
        kDouble = 1,
        kFloat = 2,
        kVector = 3,
        kString = 4,
        kInt64 = 5,
        kUint64 = 6,
        kInt32 = 7,
        kUint32 = 8,
        kInt16 = 9,
        kUint16 = 10,
        kInt8 = 11,
        kUint8 = 12,
        kBool = 13,
        kMaxBuildInProtoId = 14,
    };
    // todo: constexpr array
    inline static std::unordered_map<std::type_index, BuildInProtoId> protoIdMap = {
        {typeid(double), BuildInProtoId::kDouble},
        {typeid(float), BuildInProtoId::kFloat},
        {typeid(std::vector<std::any>), BuildInProtoId::kVector},
        {typeid(std::string), BuildInProtoId::kString},
        {typeid(int64_t), BuildInProtoId::kInt64},
        {typeid(uint64_t), BuildInProtoId::kUint64},
        {typeid(int32_t), BuildInProtoId::kInt32},
        {typeid(uint32_t), BuildInProtoId::kUint32},
        {typeid(int16_t), BuildInProtoId::kInt16},
        {typeid(uint16_t), BuildInProtoId::kUint16},
        {typeid(int8_t), BuildInProtoId::kInt8},
        {typeid(uint8_t), BuildInProtoId::kUint8},
        {typeid(bool), BuildInProtoId::kBool},
    };
    static constexpr uint64_t UuidBit = 32;
    static constexpr uint64_t ProtoUuidMask = (uint64_t(1) << UuidBit) - 1;
    explicit Context(uint32_t machineId): machinePrefix(uint64_t(machineId) << UuidBit), proto(), dirtyProto() {
        counter = (machineId == 0) ? BuildInProtoId::kMaxBuildInProtoId : 0;
    }
    struct ValueInfo {
        uint64_t protoId;
        uint64_t memberId;
    };
    std::unordered_map<uint64_t, std::map<std::string, ValueInfo>> proto;
    std::set<uint64_t> dirtyProto;
    uint64_t newUuid() {
        uint64_t uuid = counter++;
        uuid |= machinePrefix;
        dirtyProto.insert(uuid);
        return uuid;
    };
private:
    uint64_t machinePrefix;
    uint64_t counter;
};

/**
 * @brief serialize CSValueMap to buffer
 * 
 * @param ctx context to storage proto
 * @param protoId proto id of current data
 * @param data CSValueMap need to serialize
 * @param buffer buffer to store serialized data
 * @return is success
 */
bool serialize(
    Context& ctx, 
    uint64_t protoId, 
    const std::unordered_map<std::string, std::any>& data, 
    std::vector<std::byte>& buffer
);

}

#endif