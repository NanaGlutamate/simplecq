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
* sim controller - 主程序，加载yml格式的想定文件

# 用户评价

![4a6623d622a1af4319e06e0f25f52831](https://github.com/user-attachments/assets/48a833e1-d73a-4512-b755-04a908179475)

# 使用方法

you can download pre-build version in [workflow](https://github.com/NanaGlutamate/simplecq/actions)

## assembled model (mymodel.dll)

组合模型，与CQ中组合模型基本一致，加载组合模型描述文件（如```config/assamble.yml```）

### 模型重启

在原本模型的基础上支持模型级别的重启，如果：

1. 组合模型描述文件中```config```项包含```restart_key```
2. 当前帧通过任意一次```SetInput```调用收到的主题数据包含上述```restart_key```指定的值

那么组合模型将会在下一帧的```GetOutput```函数调用之前释放并重新创建当前管理的原子模型、以想定最开始的```Init```函数调用中传入的初始化参数重新初始化各个原子模型

### 组合模型描述文件



## agent model (agent.dll)

代理模型，利用socket与训练环境通信，包含数据序列化与反序列化

### python接口

gym环境风格的代理模型python接口，配置代理模型ip与端口后与其建立连接，在python环境与仿真环境间转发数据

## sim controller (tinycq.exe)

类似于CQSim的运控工具，基于taskflow完成了并行化，支持发布订阅模型下的数据交互

目前只能支持基本的仿真推演与基于ASCII字符画的可视化，暂未支持的功能：
1. 引擎的RPC控制
2. 模型数据检查
3. 分布式仿真

### 使用流程

仿真控制器为交互式的控制台工具，主要使用流程为：

1. 使用```load [filepath]```命令加载想定描述文件
2. 使用```edit [config] [value]```命令修改仿真设置
3. 使用```r [times]```命令开始仿真运行

控制器接受的其他命令可以通过输入```help```获取

### 想定描述文件

想定描述文件包括以下部分：
1. ```model_types```：想定涉及的模型类型数组，每一项包含以下成员：
   1. ```model_type_name```：模型类型的名称，后续在模型实例与模型间交互关系设计中需要引用（为保证与CQ的兼容性，发布订阅关系仍然为模型级别）
   2. ```dll_path```：模型动态库路径
   3. ```output_movable```：模型输出能否使用移动语义优化，设置为```false```能够保证正确性；如果建模人员确定能够设置为```true```则可以设置为```true```以提高性能
2. ```models```：想定涉及的模型实例数组，每一项包含以下成员：
   1. ```model_type```：模型类型的名称
   2. ```side_id```：阵营ID
   3. ```id```：模型ID
   4. ```init_value```：初始化参数，以XML格式描述，详见附件 
        // TODO:
3. ```topics```：想定交互关系设计，同样采用发布订阅模型，每一项为一个主题，包含以下成员：
   1. ```from```：主题发布者
   2. ```members```：成员名称，为保证兼容性，仅当调用发布者的输出函数```GetOutput```得到的```CSValueMap```包含```members```中的全部成员时会收集对应数据组装为一个主题数据
   3. ```subscribers```：主题的订阅者列表，包含：
      1. ```to```：订阅者模型类型
      2. ```name_convert```：名称转换关系，即将主题中的名称转换为对应模型```SetInput```函数接受的名称

### 性能分析

to collect taskflow profile, run
```shell
$env:TF_ENABLE_PROFILER="[PATH_TO_TARGET_DIR]\simple.json"
```
before run tinycq.exe

### 可视化

TODO: 

# 文件结构

## src/

主要源码目录，包含文件：

1. agent.cpp：代理模型源码
2. dllop.cpp：DLL相关操作
3. gym_interface.py：代理模型的python接口
4. main.cpp：暂时弃用
5. model.cpp：代理模型源码
6. mysock.cpp：socket通信相关源码
7. test.cpp：测试项目
8. tinycq.cpp：tinycq.exe主程序

### src/agentrpc/

暂时弃用

### src/engine/

tinycq控制台接口相关源码

## include/

头文件目录

### include/engine/

tinycq控制台接口相关头文件

## config/

配置文件，主要来自示例想定

## 3rd-party/

第三方源码与其开源协议
