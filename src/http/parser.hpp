#pragma once
#include "request.hpp"
#include "response.hpp"
#include <string>
#include <string_view>
#include <optional>

namespace proxy::http {

class HttpParser {
public:
    enum class ParseState {
        INCOMPLETE,
        COMPLETE,
        ERROR
    };

    struct ParseResultRequest {
        ParseState state;
        std::optional<HttpRequest> request;
        size_t bytes_consumed = 0;
    };

    struct ParseResultResponse {
        ParseState state;
        std::optional<HttpResponse> response;
        size_t bytes_consumed = 0;
    };

    static ParseResultRequest parse_request(const std::string& raw);
    static ParseResultResponse parse_response(const std::string& raw);
    
    static std::string serialize_request(const HttpRequest& req);
    static std::string serialize_response(const HttpResponse& res);
};

}
