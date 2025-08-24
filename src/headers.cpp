#include "headers.h"

#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    // code here
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    auto host_pos = req.find("Host:");
    if (host_pos == std::string_view::npos) {
        throw std::runtime_error("Host header not found");
    }

    // Start after "Host:"
    auto value_start = host_pos + 5;

    // Find colon in the value part (not the one in "Host:")
    auto delim_pos = req.find(':', value_start);
    if (delim_pos == std::string_view::npos) {
        throw std::runtime_error("No port found in Host header");
    }

    // Extract host: from value_start to delim_pos
    std::string host = std::string(req.substr(value_start, delim_pos - value_start));

    // Find end of line (where port ends)
    auto line_end = req.find("\r\n", delim_pos);
    if (line_end == std::string_view::npos) {
        line_end = req.size();
    }

    // Extract port: from delim_pos+1 to line_end
    std::string port = std::string(req.substr(delim_pos + 1, line_end - delim_pos - 1));

    return {host, port};
}

/*std::optional<size_t> findContentLength(std::string_view rsp) {
    // code here
}*/
