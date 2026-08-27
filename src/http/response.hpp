#pragma once
#include <string>
#include <unordered_map>
#include "request.hpp" // for case-insensitive hash/equal

namespace proxy::http {

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string version = "HTTP/1.1";
    std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> headers;
    std::string body;
};

}
