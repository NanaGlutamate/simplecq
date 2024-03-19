// #include <fkYAML/node.hpp>
#include <yaml-cpp/yaml.h>

#include <array>
#include <format>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include <assert.h>

#include "agentrpc/cq_agent.pb.h"

#include "csmodel_base.h"
#include "printcsvaluemap.hpp"
#include "parseany.hpp"

namespace {

using namespace std::literals;

using CSValueMap = std::unordered_map<std::string, std::any>;

} // namespace

class AgentModel : public CSModelObject {
  public:
    AgentModel() = default;
    virtual bool Init(const CSValueMap &value) override {
        SetState(CSInstanceState::IS_INITIALIZED);
        return true;
    };

    virtual bool Tick(double time) override {
        // send: input end
        return true;
    };

    virtual bool SetInput(const CSValueMap &value) override {
        auto s = tools::myany::printCSValueMapToString<tools::myany::PythonFormat>(value);
        // send
        return true;
    };

    virtual CSValueMap *GetOutput() override {
        SetState(CSInstanceState::IS_RUNNING);
        std::string s;
        // recv
        auto v = tools::myany::parseXMLString(s).transform_error([this](auto err){
            this->WriteLog(std::format("parse error: {}", err), 4);
            return CSValueMap{};
        }).value();
        buffer = std::any_cast<CSValueMap>(std::move(v));
        buffer.emplace("ForceSideID", GetForceSideID());
        buffer.emplace("ModelID", GetModelID());
        buffer.emplace("InstanceName", GetInstanceName());
        buffer.emplace("ID", GetID());
        buffer.emplace("State", uint16_t(GetState()));
        return &buffer;
    };
  private:
    CSValueMap buffer;
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