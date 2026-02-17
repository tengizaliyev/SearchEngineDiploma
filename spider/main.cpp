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

std::string downloadPage(const std::string& host)
{
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);

    auto const results = resolver.resolve(host, "80");
    stream.connect(results);

    http::request<http::string_body> req{http::verb::get, "/", 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "DiplomaSpiderBot");

    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    return res.body();
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

    std::string url = "http://neverssl.com";
    std::cout << "Downloading: " << url << "\n";

    std::string html = downloadPage("neverssl.com");

    db.saveDocument(url, html);

    std::cout << "Spider finished!\n";
}
