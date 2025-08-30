#include "headers.h"

#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    size_t headers_start = 0;

    // Полный http реквест? (начинается с http метода)
    if (req.substr(0, 3) == "GET" || req.substr(0, 4) == "POST" || req.substr(0, 3) == "PUT" ||
        req.substr(0, 6) == "DELETE") {
        // Skip the request line (everything until the first \r\n)
        auto line_end = req.find("\r\n");
        if (line_end == std::string_view::npos) {
            return;
        }
        headers_start = line_end + 2;  // Skip \r\n
    }

    auto headers_end = req.find("\r\n\r\n", headers_start);
    if (headers_end == std::string_view::npos) {
        headers_end = req.size();
    } else {
        headers_end += 2;  // Include the first \r\n of the double CRLF
    }

    std::string_view headers_block = req.substr(headers_start, headers_end - headers_start);

    size_t pos = 0;
    while (pos < headers_block.size()) {
        auto colon_pos = headers_block.find(':', pos);
        if (colon_pos == std::string_view::npos) {
            break;
        }

        auto line_end_pos = headers_block.find("\r\n", pos);
        if (line_end_pos == std::string_view::npos) {
            line_end_pos = headers_block.size();
        }

        std::string_view key = headers_block.substr(pos, colon_pos - pos);
        std::string_view value = headers_block.substr(colon_pos + 1, line_end_pos - colon_pos - 1);

        // Trim whitespace from value
        while (!value.empty() && std::isspace(value[0])) {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(value.back())) {
            value.remove_suffix(1);
        }

        callback(key, value);

        pos = line_end_pos + 2;  // Move to next line
        if (pos >= headers_block.size()) {
            break;
        }

        // Check if we've reached the end of headers (empty line)
        if (pos + 2 <= headers_block.size() && headers_block.substr(pos, 2) == "\r\n") {
            break;
        }
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string_view str_to_find = "Host: ";
    auto host_pos = req.find(str_to_find);
    if (host_pos == std::string_view::npos) {
        throw std::runtime_error("Host header not found");
    }

    auto value_start = host_pos + str_to_find.size();

    auto delim_pos = req.find(':', value_start);
    if (delim_pos == std::string_view::npos) {
        throw std::runtime_error("No port found in Host header");
    }

    std::string host = std::string(req.substr(value_start, delim_pos - value_start));

    auto line_end = req.find("\r\n", delim_pos);
    if (line_end == std::string_view::npos) {
        line_end = req.size();
    }

    std::string port = std::string(req.substr(delim_pos + 1, line_end - delim_pos - 1));

    return {host, port};
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::string_view str_to_find = "Content-Length: ";
    auto content_pos = rsp.find(str_to_find);
    if (content_pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto value_start = content_pos + str_to_find.size();
    auto line_end = rsp.find("\r\n", value_start);

    std::string length_str = std::string(rsp.substr(value_start, line_end - value_start));

    return std::stoull(length_str);
}
