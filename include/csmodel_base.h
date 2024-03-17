/**
 * @file csmodel_base.h
 * @author djw
 * @brief CQ/Interpreter/CQ Base
 * @date 2023-03-28
 * 
 * @details CQ interface for model
 * 
 * @par history
 * <table>
 * <tr><th>Author</th><th>Date</th><th>Changes</th></tr>
 * <tr><td>djw</td><td>2023-03-28</td><td>Initial version.</td></tr>
 * </table>
 */
#ifndef CS_MODEL_BASE_H
#define CS_MODEL_BASE_H

#include <any>
#include <exception>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

enum class CSInstanceState {
    IS_UNSPECIFIED = 0,
    IS_CREATED = 1,     // 实例被创建
    IS_INITIALIZED = 2, // 实例完成初始化
    IS_RUNNING = 3,     // 实例运行中
    IS_STOPPED = 4,     // 实例停止运算但暂不需销毁
    IS_DESTROYED = 5,   // 实例等候销毁
    IS_ERROR = 6        // 实例内部运算发生错误
};

// 模型基类
class CSModelObject {
  public:
    // 模型基本接口
    // 模型需实现以下接口
    // 以下接口由引擎调用

    // 模型实例初始化(一次性传递所有初始化参数)
    virtual bool Init(const std::unordered_map<std::string, std::any> &value) = 0;
    // 模型实例单步计算
    virtual bool Tick(double time) = 0;
    // 设置模型实例的输入参数
    virtual bool SetInput(const std::unordered_map<std::string, std::any> &value) = 0;
    // 获取模型所有输出参数
    virtual std::unordered_map<std::string, std::any> *GetOutput() = 0;

  public:
    // 模型基本信息接口
    // 以下接口由引擎和模型内部逻辑调用
    // 提供内置模型参数的访问接口
    // 统一按Set,Get形式描述

    // 设置模型实例状态
    void SetState(CSInstanceState state) { state_ = state; }
    // 获取模型实例状态
    CSInstanceState GetState(void) { return state_; }

    // 设置模型实例ID
    void SetID(uint64_t id) { id_ = id; }
    // 获取模型实例ID
    uint64_t GetID() const { return id_; }

    // 设置模型实例所属模型唯一标识(模型类型ID)
    void SetModelID(const std::string &model_id) { model_id_ = model_id; }
    // 获取模型实例所属模型唯一标识(模型类型ID)
    const std::string &GetModelID() const { return model_id_; }

    // 设置模型实例的名称
    void SetInstanceName(const std::string &instance_name) { instance_name_ = instance_name; }
    // 获取模型实例的名称
    const std::string &GetInstanceName() const { return instance_name_; }

    // 设置模型实例所属的阵营的唯一标识
    void SetForceSideID(uint16_t force_side_id) { force_side_id_ = force_side_id; }
    // 获取模型实例所属的阵营的唯一标识
    uint16_t GetForceSideID() const { return force_side_id_; }

  protected:
    // 与以上接口相对应的成员变量

    // 模型实例状态
    CSInstanceState state_ = CSInstanceState::IS_UNSPECIFIED;
    // 模型实例ID
    uint64_t id_;
    // 模型类型ID
    std::string model_id_;
    // 模型实例名称
    std::string instance_name_;
    // 所属阵营ID
    uint16_t force_side_id_;

  protected:
    // 其它辅助方法
    // 以下接口由模型内部逻辑调用

    // 输出日志 (0-Trace/1-Debug/2-Info/3-Warn/4-Error/5-Critical Error)
    void WriteLog(const std::string &msg, uint32_t level = 0) { log_(msg, level); }

    // 输出业务消息
    void WriteKeyMessage(const std::vector<std::any> &msgs) { params_.emplace("KeyMessages", msgs); }

    // 模型实例直接对外写主题 (待调整)
    void WriteTopic(const std::string &topic_name, const std::unordered_map<std::string, std::any> &topic_data) {
        std::unordered_map<std::string, std::any> topic;
        topic["TopicName"] = topic_name;
        topic["Params"] = topic_data;
        com_cb_("DirectWriteTopic", topic);
    }

    // 动态创建实体
    void CreateEntity(const std::string &model_id, const std::string &instance_name, uint64_t instance_id,
                      uint16_t forceside_id, std::unordered_map<std::string, std::any> &params) {
        std::unordered_map<std::string, std::any> basic_params = {
            {"InstanceName", instance_name}, {"ID", instance_id}, {"ModelID", model_id}, {"ForceSideID", forceside_id}};
        basic_params.merge(params);
        com_cb_("CreateEntity", basic_params);
    }

  protected:
    // 通用回调函数
    std::string CommonCallBack(const std::string &fun_name, const std::unordered_map<std::string, std::any> &params) {
        return com_cb_(fun_name, params);
    }

  public:
    // 设置模型日志输出函数
    void SetLogFun(std::function<void(const std::string &, uint32_t)> log) { log_ = log; }
    // 设置通用回调函数
    void SetCommonCallBack(
        std::function<std::string(const std::string &, const std::unordered_map<std::string, std::any> &)> com_cb) {
        com_cb_ = com_cb;
    }

  protected:
    // 日志回调函数
    std::function<void(const std::string &, uint32_t)> log_;
    // 通用回调函数
    std::function<std::string(const std::string &, const std::unordered_map<std::string, std::any> &)> com_cb_;
    // 模型参数和值
    std::unordered_map<std::string, std::any> params_;
};

#endif
