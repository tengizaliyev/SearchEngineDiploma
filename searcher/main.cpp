#include "../include/config.hpp"
#include "../include/db.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::string extractQuery(const std::string& target)
{
    auto pos = target.find("?q=");
    if (pos == std::string::npos) return "";
    return target.substr(pos + 3);
}

int main()
{
    Config cfg;
    cfg.load("../config/settings.ini");

    std::string connStr =
        "host=" + cfg.get("db_host") +
        " port=" + cfg.get("db_port") +
        " dbname=" + cfg.get("db_name") +
        " user=" + cfg.get("db_user") +
        " password=" + cfg.get("db_password");

    Database db(connStr);

    net::io_context ioc;
    tcp::acceptor acceptor(ioc, {tcp::v4(), 8080});

    std::cout << "Searcher running: http://localhost:8080\n";

    while (true)
    {
        tcp::socket socket(ioc);
        acceptor.accept(socket);

        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        std::string query = extractQuery(std::string(req.target()));

        std::string body =
            "<html><body><h1>Search Engine</h1>"
            "<form method='GET'>"
            "<input name='q'>"
            "<button>Search</button>"
            "</form>";

        if (!query.empty())
        {
            body += "<h2>Results for: " + query + "</h2><ul>";

            auto results = db.searchWord(query);

            for (auto& url : results)
                body += "<li>" + url + "</li>";

            body += "</ul>";
        }

        body += "</body></html>";

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "text/html");
        res.body() = body;
        res.prepare_payload();

        http::write(socket, res);
    }
}
