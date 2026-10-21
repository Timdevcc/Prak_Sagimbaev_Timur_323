#include "chat.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc != 2 || std::string(argv[1]) == "--help") {
        std::cout << "Usage: local_chat config.example.json\nCommands: /reset, /exit\nInput/output: UTF-8\n";
        return argc == 2 ? 0 : 1;
    }
    try {
        std::ifstream file(argv[1]);
        if (!file) throw std::runtime_error("Cannot open configuration file");
        chat::json raw;
        try { file >> raw; }
        catch (const chat::json::parse_error& e) { throw std::runtime_error(std::string("Invalid configuration JSON: ") + e.what()); }
        const auto config = chat::parse_config(raw);
        const chat::CurlRuntime runtime;
        std::vector<chat::Message> history;
        const auto reset = [&] {
            history.clear();
            if (!config.system_prompt.empty()) history.push_back({"system", config.system_prompt});
        };
        reset();
        bool failed = false;
        std::string text;
        while (std::cout << "You> " && std::getline(std::cin, text)) {
            if (text == "/exit") break;
            if (text == "/reset") { reset(); std::cout << "History cleared.\n"; continue; }
            if (text.find_first_not_of(" \r\t\n") == std::string::npos) {
                std::cerr << "Error: enter a nonempty message.\n"; failed = true; continue;
            }
            // Work on a copy: failed requests never leave an unfinished user turn.
            auto pending = history;
            pending.push_back({"user", text});
            try {
                const auto start = std::chrono::steady_clock::now();
                int removed = 0;
                chat::json request;
                while (true) {
                    request = chat::build_request(config, pending);
                    const auto count = chat::json::parse(chat::http_post(config.server_url +
                        "/v1/chat/completions/input_tokens", request.dump(), config.timeout_seconds));
                    const int tokens = chat::nonnegative_int(count, "input_tokens");
                    if (tokens <= config.context_size - config.max_tokens) break;
                    if (!chat::remove_oldest_pair(pending))
                        throw std::runtime_error("Message exceeds context budget; shorten your input. Generation was not sent.");
                    ++removed;
                }
                const auto reply = chat::parse_reply(chat::json::parse(chat::http_post(config.server_url +
                    "/v1/chat/completions", request.dump(), config.timeout_seconds)));
                pending.push_back({"assistant", reply.text});
                history = std::move(pending);
                const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                std::cout << "Assistant> " << reply.text << "\nInput: " << reply.prompt
                    << ", answer: " << reply.completion << ", total: " << reply.total
                    << " / " << config.context_size << "; removed pairs: " << removed
                    << "; time: " << seconds << " s\n";
            } catch (const std::exception& e) {
                failed = true;
                std::cerr << "Error: " << e.what() << "\nHistory unchanged.\n";
            }
        }
        return failed ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
