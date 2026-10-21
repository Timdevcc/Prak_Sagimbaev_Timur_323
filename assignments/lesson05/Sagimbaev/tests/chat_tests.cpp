#include "chat.hpp"
#include <iostream>
#include <stdexcept>

void check(bool ok) { if (!ok) throw std::runtime_error("Check failed"); }
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected);
}
int main() {
    try {
        chat::json j = {{"server_url","http://127.0.0.1:8080/"},{"model","local-model"},
            {"system_prompt","Keep answers short"},{"context_size",1024},
            {"max_tokens",64},{"temperature",0.2},{"timeout_seconds",180}};
        auto c = chat::parse_config(j);
        check(c.server_url == "http://127.0.0.1:8080");
        auto invalid = j; invalid.erase("model"); rejects([&]{chat::parse_config(invalid);});
        invalid = j; invalid["model"] = 5; rejects([&]{chat::parse_config(invalid);});
        invalid = j; invalid["temperature"] = 3; rejects([&]{chat::parse_config(invalid);});
        invalid = j; invalid["context_size"] = 512; rejects([&]{chat::parse_config(invalid);});
        invalid = j; invalid["model"] = " "; rejects([&]{chat::parse_config(invalid);});
        std::vector<chat::Message> h = {{"system","s"},{"user","old"},{"assistant","reply"},{"user","new\n\"quote\""}};
        auto request = chat::json::parse(chat::build_request(c,h).dump());
        check(request["messages"][3]["content"] == h.back().content);
        check(request["max_tokens"] == 64 && request["stream"] == false);
        check(chat::remove_oldest_pair(h));
        check(h.size() == 2 && h[0].role == "system" && h[1].content == "new\n\"quote\"");
        check(!chat::remove_oldest_pair(h));
        h = {{"user","old"},{"assistant","r"},{"user","current"}};
        check(chat::remove_oldest_pair(h) && h.size() == 1);
        rejects([]{chat::nonnegative_int({{"input_tokens",-1}},"input_tokens");});
        rejects([]{chat::nonnegative_int({{"input_tokens",1.5}},"input_tokens");});
        rejects([]{chat::nonnegative_int({{"input_tokens",true}},"input_tokens");});
        rejects([]{chat::nonnegative_int(chat::json::object(),"input_tokens");});
        chat::json reply = {{"choices",chat::json::array({{{"message",{{"content","Hello"}}}}})},
            {"usage",{{"prompt_tokens",10},{"completion_tokens",2},{"total_tokens",12}}}};
        check(chat::parse_reply(reply).text == "Hello");
        auto bad = reply; bad.erase("usage"); rejects([&]{chat::parse_reply(bad);});
        bad = reply; bad["choices"] = chat::json::array(); rejects([&]{chat::parse_reply(bad);});
        bad = reply; bad["usage"]["total_tokens"] = 0; rejects([&]{chat::parse_reply(bad);});
        std::cout << "All checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
