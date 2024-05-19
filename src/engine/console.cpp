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

inline std::expected<void, std::string> cfg(ConsoleApp &app, const std::vector<std::string_view> &line) {
    std::cout << R"(allowed cfg: 
    loglevel: u64, 
    logfile: str, 
    enablelog: i8,
    drawrate: u64,
    dt: f64, 
)" << std::endl;
    return {};
}

inline std::expected<void, std::string> showcfg(ConsoleApp &app, const std::vector<std::string_view> &line) {
    using namespace std::literals;
    if (line[1] == "loglevel"sv) {
        std::cout << app.engine.mm.callback.log_level << std::endl;
    } else if (line[1] == "logfile"sv) {
        std::cout << app.engine.mm.callback.curr_log_file << std::endl;
    } else if (line[1] == "enablelog"sv) {
        std::cout << app.engine.mm.callback.enable_log << std::endl;
    } else if (line[1] == "drawrate"sv) {
        std::cout << app.draw_rate << std::endl;
    } else if (line[1] == "dt"sv) {
        std::cout << app.engine.s.dt << std::endl;
    } else {
        return std::unexpected("unknown cfg, input \"cfg\" to show all cfg");
    }
    return {};
}

inline std::expected<void, std::string> editcfg(ConsoleApp &app, const std::vector<std::string_view> &line) {
    using namespace std::literals;
    auto arg = std::string(line[2]);
    if (line[1] == "loglevel"sv) {
        app.engine.mm.callback.log_level = std::stoull(arg);
    } else if (line[1] == "logfile"sv) {
        app.engine.mm.callback.curr_log_file = arg;
        app.engine.mm.callback.log_file = std::ofstream(arg);
    } else if (line[1] == "enablelog"sv) {
        app.engine.mm.callback.enable_log = std::stoi(arg);
    } else if (line[1] == "drawrate"sv) {
        app.draw_rate = std::stoull(arg);
    } else if (line[1] == "dt"sv) {
        app.engine.s.dt = std::stod(arg);
    } else {
        return std::unexpected("unknown cfg, input \"cfg\" to show all cfg");
    }
    return {};
}

}; // namespace

// TODO: reload file, store cfg / load cfg file

std::map<std::string, ConsoleApp::Command, std::less<>> ConsoleApp::commandCallbacks{
    {"load", {1, load}},    {"run", {1, run}}, {"cfg", {0, cfg}}, {"show", {1, showcfg}},
    {"edit", {2, editcfg}}, {"l", {1, load}},  {"r", {1, run}},
};