#include "engine/console.hpp"
#include "engine/executionengine.hpp"
#include "engine/modelmanager.hpp"
#include "engine/coroutined.hpp"

int main() {
    ConsoleApp app{};
    app.initCfg();
    app.processCommand("l D:/Desktop/FinalProj/Code/simplecq/config/scene_carbattle.yml")
        .or_else([](const std::string &err) -> std::expected<void, std::string> {
            std::cout << err << std::endl;
            return {};
        });
    app.engine.frame.dump(std::cout);
    app.draw_rate = 0;
    app.processCommand("r 2000").or_else([](const std::string &err) -> std::expected<void, std::string> {
        std::cout << err << std::endl;
        return {};
    });
    app.draw();
    return 0;
}