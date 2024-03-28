// #include <fkYAML/node.hpp>
#include <yaml-cpp/yaml.h>

#include <array>
#include <format>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>

#include <assert.h>

// #include <grpc/grpc.h>
// #include <grpcpp/channel.h>
// #include <grpcpp/client_context.h>
// #include <grpcpp/create_channel.h>
// #include <grpcpp/security/credentials.h>

// #include "agentrpc/cq_agent.pb.h"

#include "csmodel_base.h"
#include "mysock.hpp"
#include "parseany.hpp"
#include "printcsvaluemap.hpp"

namespace {

using namespace std::literals;

using CSValueMap = std::unordered_map<std::string, std::any>;

constexpr auto host = "127.0.0.1";

} // namespace

class AgentModel : public CSModelObject {
  public:
    AgentModel() = default;
    virtual bool Init(const CSValueMap &value) override {
        auto port = std::any_cast<uint32_t>(value.find("port")->second);
        SetState(CSInstanceState::IS_INITIALIZED);
        while (!l.link(host, port)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        auto s = tools::myany::printAnyToString<tools::myany::PythonFormat>(value);
        l.sendValue("[" + s + "]");
        return true;
    };

    virtual bool Tick(double time) override {
        auto s = tools::myany::printAnyToString<tools::myany::PythonFormat>(std::move(inputBuffer));
        inputBuffer.clear();
        l.sendValue(s);
        return true;
    };

    virtual bool SetInput(const CSValueMap &value) override {
        inputBuffer.push_back(value);
        return true;
    };

    virtual CSValueMap *GetOutput() override {
        SetState(CSInstanceState::IS_RUNNING);
        std::string s = l.getValue();
        try {
            auto v = tools::myany::parseXMLString(s).or_else(
                [this](auto err) -> std::expected<std::any, tools::myany::parseError> {
                    this->WriteLog(std::format("parse error: {}", err), 4);
                    return CSValueMap{};
                });
            outputBuffer = std::any_cast<CSValueMap>(std::move(v.value()));
        } catch (rapidxml::parse_error &err) {
            WriteLog(err.what(), 5);
            WriteLog(s, 5);
        }
        outputBuffer.emplace("ForceSideID", GetForceSideID());
        outputBuffer.emplace("ModelID", GetModelID());
        outputBuffer.emplace("InstanceName", GetInstanceName());
        outputBuffer.emplace("ID", GetID());
        outputBuffer.emplace("State", uint16_t(GetState()));
        return &outputBuffer;
    };

  private:
    Link l;
    std::vector<std::any> inputBuffer;
    CSValueMap outputBuffer;
};

extern "C" {
__declspec(dllexport) CSModelObject *CreateModelObject() { return new AgentModel; };
__declspec(dllexport) void DestroyMemory(void *mem, bool is_array) {
    if (is_array) {
        delete[] ((AgentModel *)mem);
    } else {
        delete (AgentModel *)mem;
    }
};
}