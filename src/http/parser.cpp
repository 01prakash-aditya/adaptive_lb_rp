#include "parser.hpp"
#include <sstream>
#include <algorithm>

namespace proxy::http {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

HttpParser::ParseResultRequest HttpParser::parse_request(const std::string& raw) {
    ParseResultRequest result{ParseState::INCOMPLETE, std::nullopt, 0};
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return result;

    HttpRequest req;
    std::string header_section = raw.substr(0, header_end);
    std::istringstream stream(header_section);
    std::string line;
    
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream req_line(line);
        req_line >> req.method >> req.uri >> req.version;
    } else {
        result.state = ParseState::ERROR;
        return result;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string val = trim(line.substr(colon + 1));
            req.headers[key] = val;
        }
    }

    size_t body_start = header_end + 4;
    
    if (auto it = req.headers.find("Transfer-Encoding"); it != req.headers.end() && it->second == "chunked") {
        size_t current_pos = body_start;
        std::string decoded_body;
        while (current_pos < raw.size()) {
            size_t next_crlf = raw.find("\r\n", current_pos);
            if (next_crlf == std::string::npos) return result;
            std::string chunk_size_str = raw.substr(current_pos, next_crlf - current_pos);
            size_t chunk_size = 0;
            try {
                chunk_size = std::stoul(chunk_size_str, nullptr, 16);
            } catch (...) {
                result.state = ParseState::ERROR;
                return result;
            }
            if (chunk_size == 0) {
                if (raw.size() < next_crlf + 4) return result;
                result.state = ParseState::COMPLETE;
                req.body = decoded_body;
                result.request = std::move(req);
                result.bytes_consumed = next_crlf + 4;
                return result;
            }
            if (raw.size() < next_crlf + 2 + chunk_size + 2) return result;
            decoded_body.append(raw.substr(next_crlf + 2, chunk_size));
            current_pos = next_crlf + 2 + chunk_size + 2;
        }
        return result;
    } 
    else if (auto it = req.headers.find("Content-Length"); it != req.headers.end()) {
        size_t content_length = std::stoull(it->second);
        if (raw.size() < body_start + content_length) {
            return result;
        }
        req.body = raw.substr(body_start, content_length);
        result.bytes_consumed = body_start + content_length;
    } else {
        result.bytes_consumed = body_start;
    }

    result.state = ParseState::COMPLETE;
    result.request = std::move(req);
    return result;
}

HttpParser::ParseResultResponse HttpParser::parse_response(const std::string& raw) {
    ParseResultResponse result{ParseState::INCOMPLETE, std::nullopt, 0};
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return result;

    HttpResponse res;
    std::string header_section = raw.substr(0, header_end);
    std::istringstream stream(header_section);
    std::string line;
    
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream req_line(line);
        req_line >> res.version >> res.status_code;
        std::string dummy;
        std::getline(req_line, dummy);
        res.status_text = trim(dummy);
    } else {
        result.state = ParseState::ERROR;
        return result;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string val = trim(line.substr(colon + 1));
            res.headers[key] = val;
        }
    }

    size_t body_start = header_end + 4;
    
    if (auto it = res.headers.find("Transfer-Encoding"); it != res.headers.end() && it->second == "chunked") {
        size_t current_pos = body_start;
        std::string decoded_body;
        while (current_pos < raw.size()) {
            size_t next_crlf = raw.find("\r\n", current_pos);
            if (next_crlf == std::string::npos) return result;
            std::string chunk_size_str = raw.substr(current_pos, next_crlf - current_pos);
            size_t chunk_size = 0;
            try {
                chunk_size = std::stoul(chunk_size_str, nullptr, 16);
            } catch (...) {
                result.state = ParseState::ERROR;
                return result;
            }
            if (chunk_size == 0) {
                if (raw.size() < next_crlf + 4) return result;
                result.state = ParseState::COMPLETE;
                res.body = decoded_body;
                result.response = std::move(res);
                result.bytes_consumed = next_crlf + 4;
                return result;
            }
            if (raw.size() < next_crlf + 2 + chunk_size + 2) return result;
            decoded_body.append(raw.substr(next_crlf + 2, chunk_size));
            current_pos = next_crlf + 2 + chunk_size + 2;
        }
        return result;
    } 
    else if (auto it = res.headers.find("Content-Length"); it != res.headers.end()) {
        size_t content_length = std::stoull(it->second);
        if (raw.size() < body_start + content_length) {
            return result;
        }
        res.body = raw.substr(body_start, content_length);
        result.bytes_consumed = body_start + content_length;
    } else {
        res.body = raw.substr(body_start);
        result.bytes_consumed = raw.size();
    }

    result.state = ParseState::COMPLETE;
    result.response = std::move(res);
    return result;
}

std::string HttpParser::serialize_request(const HttpRequest& req) {
    std::ostringstream oss;
    oss << req.method << " " << req.uri << " " << req.version << "\r\n";
    for (const auto& [k, v] : req.headers) {
        oss << k << ": " << v << "\r\n";
    }
    if (!req.body.empty() && req.headers.find("Content-Length") == req.headers.end() && req.headers.find("Transfer-Encoding") == req.headers.end()) {
        oss << "Content-Length: " << req.body.size() << "\r\n";
    }
    oss << "\r\n" << req.body;
    return oss.str();
}

std::string HttpParser::serialize_response(const HttpResponse& res) {
    std::ostringstream oss;
    oss << res.version << " " << res.status_code << " " << res.status_text << "\r\n";
    for (const auto& [k, v] : res.headers) {
        oss << k << ": " << v << "\r\n";
    }
    if (!res.body.empty() && res.headers.find("Content-Length") == res.headers.end() && res.headers.find("Transfer-Encoding") == res.headers.end()) {
        oss << "Content-Length: " << res.body.size() << "\r\n";
    }
    oss << "\r\n" << res.body;
    return oss.str();
}

}
