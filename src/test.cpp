#include <string>

#include "csmodel_base.h"
#include "dllop.hpp"

int main(){
    ModelDllInterface md{"./mymodel.dll"};
    auto p = md.createFunc();
    p->Init({});
    auto& output = *p->GetOutput();
    p->SetInput({});
    p->Tick(0.01);
    return 0;
}