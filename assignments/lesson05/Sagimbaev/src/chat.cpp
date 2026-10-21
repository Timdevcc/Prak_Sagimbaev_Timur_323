#include "chat.hpp"
#include <limits>
#include <stdexcept>

namespace chat {
int nonnegative_int(const json& obj, const std::string& key) {
    if (!obj.is_object() || !obj.contains(key) || !obj.at(key).is_number_integer()
        || obj.at(key) < 0 || obj.at(key) > std::numeric_limits<int>::max())
        throw std::runtime_error("Expected nonnegative integer: " + key);
    return obj.at(key).get<int>();
}
static std::string string_field(const json& obj, const char* key, bool empty = false) {
    if (!obj.is_object() || !obj.contains(key) || !obj.at(key).is_string())
        throw std::runtime_error(std::string("Expected string: ") + key);
    auto result = obj.at(key).get<std::string>();
    if (!empty && result.find_first_not_of(" \r\n\t") == std::string::npos)
        throw std::runtime_error(std::string("Empty field: ") + key);
    return result;
}
Config parse_config(const json& j) {
    Config c;
    c.server_url = string_field(j, "server_url");
    if (c.server_url.rfind("http://", 0) != 0 && c.server_url.rfind("https://", 0) != 0)
        throw std::runtime_error("server_url must start with http:// or https://");
    while (c.server_url.back() == '/') c.server_url.pop_back();
    c.model = string_field(j, "model");
    c.system_prompt = string_field(j, "system_prompt", true);
    c.context_size = nonnegative_int(j, "context_size");
    c.max_tokens = nonnegative_int(j, "max_tokens");
    c.timeout_seconds = nonnegative_int(j, "timeout_seconds");
    if (c.context_size != 1024 || c.max_tokens != 64)
        throw std::runtime_error("This assignment requires context_size=1024 and max_tokens=64");
    if (c.timeout_seconds < 1 || c.timeout_seconds > 600)
        throw std::runtime_error("timeout_seconds must be 1..600");
    if (!j.contains("temperature") || !j.at("temperature").is_number())
        throw std::runtime_error("Expected number: temperature");
    c.temperature = j.at("temperature").get<double>();
    if (!(c.temperature >= 0 && c.temperature <= 2))
        throw std::runtime_error("temperature must be 0..2");
    return c;
}
json build_request(const Config& c, const std::vector<Message>& history) {
    auto messages = json::array();
    for (const auto& m : history) messages.push_back({{"role", m.role}, {"content", m.content}});
    return {{"model", c.model}, {"messages", messages}, {"stream", false},
            {"max_tokens", c.max_tokens}, {"temperature", c.temperature}};
}
bool remove_oldest_pair(std::vector<Message>& h) {
    const std::size_t first = !h.empty() && h.front().role == "system" ? 1 : 0;
    if (h.size() < first + 3 || h[first].role != "user" || h[first+1].role != "assistant") return false;
    h.erase(h.begin() + first, h.begin() + first + 2);
    return true;
}
Reply parse_reply(const json& j) {
    if (!j.is_object() || !j.contains("choices") || !j.at("choices").is_array()
        || j.at("choices").empty() || !j.at("choices")[0].is_object()
        || !j.at("choices")[0].contains("message"))
        throw std::runtime_error("Missing choices[0].message");
    auto text = string_field(j.at("choices")[0].at("message"), "content");
    if (!j.contains("usage")) throw std::runtime_error("Missing usage statistics");
    const auto& u = j.at("usage");
    Reply r{text, nonnegative_int(u,"prompt_tokens"), nonnegative_int(u,"completion_tokens"), nonnegative_int(u,"total_tokens")};
    if (static_cast<long long>(r.prompt) + r.completion != r.total)
        throw std::runtime_error("Inconsistent usage statistics");
    return r;
}
}
