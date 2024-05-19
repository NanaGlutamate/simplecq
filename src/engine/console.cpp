#include "engine/console.hpp"

namespace {

inline std::expected<void, std::string> load(ConsoleApp &app, const std::vector<std::string_view> &line) {
    return app.loadFile(std::string(line[1]));
}

inline std::expected<void, std::string> run(ConsoleApp &app, const std::vector<std::string_view> &line) {
    auto times = std::stoull(std::string(line[1]));
    if (app.draw_rate == 0) {
        app.engine.run(times);
    } else {
        while (times > app.draw_rate) {
            app.engine.run(app.draw_rate);
            times -= app.draw_rate;
            app.draw();
        }
        app.engine.run(times);
    }
    return {};
}

inline std::expected<void, std::string> allcfg(ConsoleApp &app, const std::vector<std::string_view> &line) {
    //     std::cout << R"(allowed cfg:
    //     loglevel: u64,
    //     logfile: str,
    //     enablelog: i8,
    //     drawrate: u64,
    //     dt: f64,
    // )" << std::endl;
    std::cout << "avaliable cfg: ";
    for (auto &&[k, v] : app.cfg.getCallBacks()) {
        std::cout << k << ", ";
    }
    std::cout << std::endl;
    return {};
}

inline std::expected<void, std::string> showcfg(ConsoleApp &app, const std::vector<std::string_view> &line) {
    using namespace std::literals;
    auto key = std::string(line[1]);
    if (app.cfg.isKeyListened(key)) {
        std::cout << app.cfg.getValue(key) << std::endl;
        return {};
    }
    return std::unexpected("unknown cfg, input \"cfg\" to show all cfg");
}

inline std::expected<void, std::string> editcfg(ConsoleApp &app, const std::vector<std::string_view> &line) {
    using namespace std::literals;
    auto key = std::string(line[1]);
    auto arg = std::string(line[2]);
    if (app.cfg.isKeyListened(key)) {
        app.cfg.setValue(key, arg);
        return {};
    }
    return std::unexpected("unknown cfg, input \"cfg\" to show all cfg");
}

inline std::expected<void, std::string> print(ConsoleApp &app, const std::vector<std::string_view> &line) {
    app.printBuffer();
    return {};
}

}; // namespace

// TODO: reload file, store cfg / load cfg file

std::map<std::string, ConsoleApp::Command, std::less<>> ConsoleApp::commandCallbacks{
    {"load", {1, load}},   {"l", {1, load}},      {"run", {1, run}},     {"r", {1, run}},   {"cfg", {0, allcfg}},
    {"get", {1, showcfg}}, {"set", {2, editcfg}}, {"print", {0, print}}, {"p", {0, print}},
};

void ConsoleApp::initCfg() {
    cfg.listen("loglevel", [this](auto &arg) { engine.mm.callback.log_level = std::stoull(arg); });
    cfg.listen("logfile", [this](auto &arg) { engine.mm.callback.log_file = std::ofstream(arg); });
    cfg.listen("enablelog", [this](auto &arg) { engine.mm.callback.enable_log = std::stoi(arg); });
    cfg.listen("drawrate", [this](auto &arg) { draw_rate = std::stoull(arg); });
    cfg.listen("dt", [this](auto &arg) { engine.s.dt = std::stod(arg); });

    cfg.syncWithFile("engine.ini");

    cfg.setValue("loglevel", std::to_string(engine.mm.callback.log_level));
    cfg.setValue("enablelog", std::to_string(engine.mm.callback.enable_log));
    cfg.setValue("drawrate", std::to_string(draw_rate));
    cfg.setValue("dt", std::to_string(engine.s.dt));
}