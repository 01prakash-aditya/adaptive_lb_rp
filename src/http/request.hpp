#pragma once
#include <string>
#include <unordered_map>
#include <random>
#include <sstream>
#include <iomanip>

namespace proxy::http {

struct CaseInsensitiveHash {
    size_t operator()(const std::string& str) const {
        size_t h = 0;
        for (char c : str) {
            h ^= std::hash<char>{}(std::tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        }
        return true;
    }
};

inline std::string generate_uuid_v4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version = "HTTP/1.1";
    std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> headers;
    std::string body;
    std::string request_id = generate_uuid_v4();
};

}
