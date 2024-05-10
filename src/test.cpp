#include <any>
#include <iostream>
#include <string>
#include <unordered_map>

#include <taskflow/taskflow.hpp>
#include "csmodel_base.h"
#include "dllop.hpp"

int main() {
    // tf::Taskflow t;
    // tf::Executor e;
    // std::vector<int> v(100, 0);
    // t.for_each(v.begin(), v.end(), [](auto& i){++i;});
    // e.run(t);
    // return 0;
    

    // using CS = std::unordered_map<std::string, std::any>;
    // CS init{
    //     {"filePath", std::string(__PROJECT_ROOT_PATH "/config/rule_attackside_error.xml")},
    //     {"longitude", double(114.)},
    //     {"latitude", double(36.)},
    //     {"altitude", double(0.)},
    // };
    // ModelDllInterface md = *loadDll(__PROJECT_ROOT_PATH "/config/cq_interpreter.dll");
    // auto p = md.createFunc();
    // p->SetLogFun([](const std::string& msg, auto){
    //     std::cout << msg;
    // });
    // p->Init(init);
    // for (size_t i = 0; i < 1; ++i) {
    //     if (i % 10000 == 0) {
    //         std::cout << i << '\n';
    //     }
    //     p->GetOutput();
    //     p->SetInput({});
    //     p->Tick(0.01);
    // }
    // md.destoryFunc(p, false);
    // return 0;
}