# SimpleCQ

A 
* simple(only necessary functionality included),
* tiny(2 main class),
* easy-to-debug(one process only),
* fast(20x faster in test case)
  
replacement for CQ-Sim

包含三个部分：
* assembled model - 用于替换CQSim中的组合模型，速度更快、功能更多，并且支持模型级别的重启（无需通过RPC调用平台接口）
* agent model - 代理模型，毕设使用
* sim controller - 主程序，包含与CQSim行为一致的“仿真节点”类

## assembled model (mymodel.dll)

### 描述文件结构

## agent model (agent.dll)

TODO: use asio/grpc instead of OS-provided socket.

## sim controller (tinycq.exe)

类似于CQSim的运控工具，目前没有封装为命令行工具，需要在源码中修改想定描述文件的路径（如```config/scene_carbattle.yml```）并以其为参数调用```TinyCQ```类的```Init```函数

为保证兼容性，模型初始化过程没有并行化

### 描述文件结构

### others

to collect taskflow profile, run
```shell
$env:TF_ENABLE_PROFILER="D:\Desktop\FinalProj\Code\simplecq\config\simple.json"
```
before run tinycq.exe
