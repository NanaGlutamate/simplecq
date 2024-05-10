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

-----
## assembled model (mymodel.dll)

组合模型，与CQ中组合模型基本一致，加载组合模型描述文件（如```config/assamble.yml```）

在原本模型的基础上支持模型级别的重启

-----
## agent model (agent.dll)

代理模型，利用socket与训练环境通信，包含数据序列化与反序列化


TODO: use asio/grpc instead of OS-provided socket.

-----
## sim controller (tinycq.exe)

类似于CQSim的运控工具，基于taskflow完成了并行化，支持发布订阅模型下的数据交互

目前没有封装为命令行工具，只能支持基本的仿真推演与可视化，暂未支持的功能：
1. 动态创建实例
2. 引擎的RPC控制
3. 模型数据检查

### 使用方法

仿真控制器即```TinyCQ```类，其初始化过程包括如下几步：
1. 配置```cfg```类成员，包括仿真步长、日志输出等级、日志输出路径等参数
2. 以想定描述文件的路径（如```config/scene_carbattle.yml```）为参数调用```TinyCQ::init```函数，该函数会完成模型动态库的加载、模型实例的创建及初始化。为保证兼容性，模型初始化过程没有并行化
3. 调用```TinyCQ::buildGraph```建计算图
4. 调用```TinyCQ::run```开始仿真推演，参数为运行的tick数目

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

-----
### others

to collect taskflow profile, run
```shell
$env:TF_ENABLE_PROFILER="D:\Desktop\FinalProj\Code\simplecq\config\simple.json"
```
before run tinycq.exe
