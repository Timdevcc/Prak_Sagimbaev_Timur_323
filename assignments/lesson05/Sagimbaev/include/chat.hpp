#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chat {
using nlohmann::json;
struct Message { std::string role, content; };
struct Config {
    std::string server_url, model, system_prompt;
    int context_size, max_tokens, timeout_seconds;
    double temperature;
};
Config parse_config(const json& value);
json build_request(const Config& config, const std::vector<Message>& history);
int nonnegative_int(const json& object, const std::string& key);
struct Reply { std::string text; int prompt, completion, total; };
Reply parse_reply(const json& value);
bool remove_oldest_pair(std::vector<Message>& history);
std::string http_post(const std::string& url, const std::string& payload, long timeout_seconds);
struct CurlRuntime {
    CurlRuntime();
    ~CurlRuntime();
    CurlRuntime(const CurlRuntime&) = delete;
    CurlRuntime& operator=(const CurlRuntime&) = delete;
};
}
