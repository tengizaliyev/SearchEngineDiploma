#include "../include/config.hpp"
#include "../include/db.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

Config loadConfig()
{
    Config cfg;
    if (cfg.load("config/settings.ini")) return cfg;
    if (cfg.load("../config/settings.ini")) return cfg;
    throw std::runtime_error("Cannot load config/settings.ini");
}

std::string htmlEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

int fromHex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size()) {
            int hi = fromHex(s[i + 1]);
            int lo = fromHex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
            } else {
                out.push_back(s[i]);
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string extractFormField(const std::string& body, const std::string& name)
{
    std::string needle = name + "=";
    std::size_t pos = body.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    std::size_t end = body.find('&', pos);
    std::string raw = end == std::string::npos ? body.substr(pos) : body.substr(pos, end - pos);
    return urlDecode(raw);
}

std::vector<std::string> parseQueryWords(const std::string& input)
{
    std::vector<std::string> words;
    std::string current;

    auto pushCurrent = [&]() {
        if (current.size() >= 3 && current.size() <= 32) {
            words.push_back(current);
        }
        current.clear();
    };

    for (unsigned char ch : input) {
        if (std::isalnum(ch)) {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!current.empty()) {
            pushCurrent();
            if (words.size() == 4) break;
        }
    }
    if (!current.empty() && words.size() < 4) {
        pushCurrent();
    }

    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
    return words;
}

std::string renderSearchForm()
{
    return
        "<!doctype html><html><head><meta charset='utf-8'><title>Search</title></head><body>"
        "<h1>Search Engine</h1>"
        "<form method='POST' action='/search'>"
        "<input type='text' name='q' placeholder='Enter up to 4 words' />"
        "<button type='submit'>Search</button>"
        "</form>"
        "</body></html>";
}

std::string renderResults(const std::string& query, const std::vector<std::pair<std::string, int>>& results)
{
    std::string body =
        "<!doctype html><html><head><meta charset='utf-8'><title>Results</title></head><body>"
        "<h1>Search results</h1>"
        "<p>Query: <b>" + htmlEscape(query) + "</b></p>"
        "<a href='/'>Back</a>";

    if (results.empty()) {
        body += "<p>No results found.</p>";
    } else {
        body += "<ol>";
        for (const auto& [url, score] : results) {
            body += "<li><a href='" + htmlEscape(url) + "'>" + htmlEscape(url) + "</a>"
                    " (score: " + std::to_string(score) + ")</li>";
        }
        body += "</ol>";
    }

    body += "</body></html>";
    return body;
}

std::string renderErrorPage()
{
    return
        "<!doctype html><html><head><meta charset='utf-8'><title>Error</title></head><body>"
        "<h1>Internal server error</h1>"
        "<a href='/'>Back</a>"
        "</body></html>";
}

int main()
{
    try {
        Config cfg = loadConfig();

        std::string connStr =
            "host=" + cfg.get("database.db_host", cfg.get("db_host")) +
            " port=" + cfg.get("database.db_port", cfg.get("db_port", "5432")) +
            " dbname=" + cfg.get("database.db_name", cfg.get("db_name")) +
            " user=" + cfg.get("database.db_user", cfg.get("db_user")) +
            " password=" + cfg.get("database.db_password", cfg.get("db_password"));

        Database db(connStr);
        int port = cfg.getInt("searcher.http_port", cfg.getInt("http_port", 8080));

        net::io_context ioc;
        tcp::acceptor acceptor(ioc, {tcp::v4(), static_cast<unsigned short>(port)});
        std::cout << "Searcher running: http://localhost:" << port << "\n";

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);

            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "text/html; charset=utf-8");

            try {
                if (req.method() == http::verb::get && req.target() == "/") {
                    res.body() = renderSearchForm();
                } else if (req.method() == http::verb::post && req.target() == "/search") {
                    std::string rawQuery = extractFormField(req.body(), "q");
                    auto words = parseQueryWords(rawQuery);
                    auto results = db.searchDocuments(words);
                    res.body() = renderResults(rawQuery, results);
                } else {
                    res.result(http::status::not_found);
                    res.body() = "<html><body><h1>404 Not Found</h1></body></html>";
                }
            } catch (const std::exception&) {
                res.result(http::status::internal_server_error);
                res.body() = renderErrorPage();
            }

            res.prepare_payload();
            http::write(socket, res);

            beast::error_code ec;
            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
