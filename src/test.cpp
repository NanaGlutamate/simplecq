#include "engine/console.hpp"
#include "engine/executionengine.hpp"
#include "engine/modelmanager.hpp"

int main() {
    ConsoleApp app;
    app.initCfg();
    app.processCommand("l D:/Desktop/FinalProj/Code/simplecq/config/scene_carbattle.yml")
        .or_else([](const std::string &err) -> std::expected<void, std::string> {
            std::cout << err << std::endl;
            return {};
        });
    app.engine.frame.dump(std::cout);
    app.processCommand("r 200").or_else([](const std::string &err) -> std::expected<void, std::string> {
        std::cout << err << std::endl;
        return {};
    });
    app.draw();
    return 0;
}