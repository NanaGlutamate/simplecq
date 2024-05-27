#include "engine/console.hpp"
#include "engine/executionengine.hpp"
#include "engine/modelmanager.hpp"

int main() {
    ConsoleApp app{};
    app.initCfg();
    app.replMode();
    // TinyCQ cq;
    // CommandReceiver cr{cq};
    // // cr.replMode();
    // cq.cfg.log_file = std::ofstream(__PROJECT_ROOT_PATH "/config/log.txt");
    // cq.cfg.log_level = 4;
    // cq.init(__PROJECT_ROOT_PATH "/config/scene_carbattle.yml");
    // cq.buildGraph();
    // // cq.frame.dump(std::cout);
    // for (int i = 0;; ++i) {
    //     cq.run(600);
    //     // cq.printBuffer();
    //     cq.draw();
    // }
    return 0;
}
