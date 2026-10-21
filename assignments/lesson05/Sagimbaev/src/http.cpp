#include "chat.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace chat {

using nlohmann::json;

// Callback вызывается из C: исключения не должны выходить за его границу.
std::size_t append_response(char* data, std::size_t size,
                            std::size_t count, void* destination) noexcept {
    if (size != 0 && count > (std::numeric_limits<std::size_t>::max)() / size) {
        return 0;
    }
    const auto bytes = size * count;
    try {
        static_cast<std::string*>(destination)->append(data, bytes);
        return bytes;
    } catch (...) {
        return 0; // libcurl сообщит об ошибке записи ответа.
    }
}

void check_curl(CURLcode result) {
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Ошибка libcurl: ") +
                                 curl_easy_strerror(result));
    }
}

std::string http_post(const std::string& url, const std::string& payload,
                      long timeout_seconds = 180) {
    // unique_ptr освобождает оба ресурса и при обычном выходе, и при исключении.
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> connection(
        curl_easy_init(), curl_easy_cleanup);
    if (!connection) {
        throw std::runtime_error("Не удалось создать HTTP-соединение");
    }
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
        curl_slist_append(nullptr, "Content-Type: application/json"),
        curl_slist_free_all);
    if (!headers) {
        throw std::runtime_error("Не удалось создать HTTP-заголовок");
    }

    std::string response;
    char details[CURL_ERROR_SIZE] = {};
    auto* curl = connection.get();
    check_curl(curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, details));
    check_curl(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()));
    check_curl(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get()));
    check_curl(curl_easy_setopt(curl, CURLOPT_POST, 1L));
    check_curl(curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str()));
    check_curl(curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L));
    check_curl(curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds));
    check_curl(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response));
    check_curl(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response));

    // payload и response остаются живы до завершения запроса.
    const auto result = curl_easy_perform(curl);
    if (result == CURLE_OPERATION_TIMEDOUT) {
        throw std::runtime_error("Истёк тайм-аут HTTP-запроса к серверу");
    }
    if (result == CURLE_COULDNT_CONNECT) {
        throw std::runtime_error("Сервер недоступен. Проверьте запуск llama-server и адрес " + url);
    }
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Ошибка HTTP-запроса: ") +
            (details[0] != '\0' ? details : curl_easy_strerror(result)));
    }

    long status = 0;
    check_curl(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status));
    if (status != 200) {
        throw std::runtime_error("Сервер вернул HTTP " + std::to_string(status) +
                                 ": " + response);
    }
    return response;
}


CurlRuntime::CurlRuntime() { check_curl(curl_global_init(CURL_GLOBAL_DEFAULT)); }
CurlRuntime::~CurlRuntime() { curl_global_cleanup(); }
}


