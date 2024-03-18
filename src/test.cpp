#include <string>
#include <unordered_map>
#include <any>

#include "csmodel_base.h"
#include "dllop.hpp"

int main(){
    using CS = std::unordered_map<std::string, std::any>;
    CS init{
        {"longitude", double(114.)},
        {"latitude", double(36.)},
        {"altitude", double(0.)},
    };
    ModelDllInterface md = *loadDll("./mymodel.dll");
    auto p = md.createFunc();
    p->Init(init);
    p->GetOutput();
    p->SetInput({});
    p->Tick(0.01);
    auto& output = *p->GetOutput();
    p->SetInput({});
    p->Tick(0.01);
    return 0;
}