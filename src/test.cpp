#include <any>
#include <string>
#include <unordered_map>
#include <iostream>

#include "csmodel_base.h"
#include "dllop.hpp"

int main() {
    using CS = std::unordered_map<std::string, std::any>;
    CS init{
        {"longitude", double(114.)},
        {"latitude", double(36.)},
        {"altitude", double(0.)},
    };
    ModelDllInterface md = *loadDll("./mymodel.dll");
    auto p = md.createFunc();
    p->Init(init);
    for (size_t i = 0; i < 10000; ++i) {
        std::cout << i << std::endl;
        p->GetOutput();
        p->SetInput({});
        p->Tick(0.01);
    }
    return 0;
}