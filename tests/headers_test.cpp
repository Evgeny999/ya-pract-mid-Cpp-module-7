#include "headers.h"
#include <gtest/gtest.h>

TEST(iterHeaders, Empty) {
    std::string_view empty = "";
    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(empty, [&](std::string_view key, std::string_view value) { headers.emplace_back(key, value); });

    EXPECT_TRUE(headers.empty());
}

TEST(iterHeaders, SkipRequestLine) {
    std::string_view request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(request, [&](std::string_view key, std::string_view value) { headers.emplace_back(key, value); });

    ASSERT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
}

TEST(iterHeaders, SingleHeader) {
    std::string_view request = "Host: example.com\r\n\r\n";
    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(request, [&](std::string_view key, std::string_view value) { headers.emplace_back(key, value); });

    ASSERT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) {
    std::string_view request = "Host: example.com\r\nContent-Type: text/html\r\nConnection: keep-alive\r\n\r\n";
    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(request, [&](std::string_view key, std::string_view value) { headers.emplace_back(key, value); });

    ASSERT_EQ(headers.size(), 3);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
    EXPECT_EQ(headers[1].first, "Content-Type");
    EXPECT_EQ(headers[1].second, "text/html");
    EXPECT_EQ(headers[2].first, "Connection");
    EXPECT_EQ(headers[2].second, "keep-alive");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::string_view request = "Set-Cookie: session=abc\r\nSet-Cookie: theme=dark\r\n\r\n";
    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(request, [&](std::string_view key, std::string_view value) { headers.emplace_back(key, value); });

    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "Set-Cookie");
    EXPECT_EQ(headers[0].second, "session=abc");
    EXPECT_EQ(headers[1].first, "Set-Cookie");
    EXPECT_EQ(headers[1].second, "theme=dark");
}

TEST(findHostPort, Simple) {
    std::string_view request = "GET / HTTP/1.1\r\nHost: example.com:8080\r\n\r\n";

    auto [host, port] = findHostPort(request);

    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "8080");
}

TEST(findHostPort, NoHost) {
    std::string_view request = "GET / HTTP/1.1\r\nContent-Type: text/html\r\n\r\n";

    EXPECT_THROW(findHostPort(request), std::runtime_error);
}

TEST(findHostPort, NoPort) {
    std::string_view request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";

    EXPECT_THROW(findHostPort(request), std::runtime_error);
}

TEST(findHostPort, WithSpaces) {
    std::string_view request = "GET / HTTP/1.1\r\nHost:   example.com  :  8080  \r\n\r\n";

    auto [host, port] = findHostPort(request);

    EXPECT_EQ(host, "  example.com  ");
    EXPECT_EQ(port, "  8080  ");
}

TEST(findContentLength, Simple) {
    std::string_view response = "HTTP/1.1 200 OK\r\nContent-Length: 1024\r\n\r\n";

    auto result = findContentLength(response);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1024);
}

TEST(findContentLength, NoContentLength) {
    std::string_view response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

    auto result = findContentLength(response);

    EXPECT_FALSE(result.has_value());
}

TEST(findContentLength, MultipleHeaders) {
    std::string_view response =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 2048\r\nConnection: keep-alive\r\n\r\n";

    auto result = findContentLength(response);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 2048);
}

TEST(findContentLength, CaseInsensitive) {
    std::string_view response = "HTTP/1.1 200 OK\r\ncontent-length: 512\r\n\r\n";

    auto result = findContentLength(response);

    EXPECT_FALSE(result.has_value());
}

TEST(findContentLength, WithSpaces) {
    std::string_view response = "HTTP/1.1 200 OK\r\nContent-Length:   4096  \r\n\r\n";

    auto result = findContentLength(response);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 4096);
}
