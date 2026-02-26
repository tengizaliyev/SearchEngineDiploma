#include "../include/config.hpp"
#include "../include/db.hpp"
#include "../include/indexer.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/ssl.h>

#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

struct UrlParts {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
};

struct Task {
    std::string url;
    int depth;
};

Config loadConfig()
{
    Config cfg;
    if (cfg.load("config/settings.ini")) return cfg;
    if (cfg.load("../config/settings.ini")) return cfg;
    throw std::runtime_error("Cannot load config/settings.ini");
}

bool parseUrl(const std::string& url, UrlParts& out)
{
    static const std::regex re(
        R"(^(https?)://([^/:?#]+)(?::(\d+))?([^?#]*)?(\?[^#]*)?.*$)",
        std::regex::icase
    );
    std::smatch m;
    if (!std::regex_match(url, m, re)) return false;

    out.scheme = m[1].str();
    out.host = m[2].str();
    out.port = m[3].str();
    std::string path = m[4].str();
    std::string query = m[5].str();

    if (out.port.empty()) {
        out.port = (out.scheme == "https") ? "443" : "80";
    }

    if (path.empty()) path = "/";
    out.target = path + query;
    return true;
}

std::string stripFragment(const std::string& url)
{
    auto pos = url.find('#');
    return pos == std::string::npos ? url : url.substr(0, pos);
}

std::string resolveUrl(const std::string& baseUrl, const std::string& href)
{
    if (href.empty()) return "";

    std::string link = stripFragment(href);
    if (link.empty()) return "";

    if (link.rfind("javascript:", 0) == 0 || link.rfind("mailto:", 0) == 0) {
        return "";
    }
    if (link.rfind("http://", 0) == 0 || link.rfind("https://", 0) == 0) {
        return link;
    }

    UrlParts base;
    if (!parseUrl(baseUrl, base)) return "";

    std::string baseOrigin = base.scheme + "://" + base.host;
    if ((base.scheme == "http" && base.port != "80") || (base.scheme == "https" && base.port != "443")) {
        baseOrigin += ":" + base.port;
    }

    if (link.rfind("//", 0) == 0) {
        return base.scheme + ":" + link;
    }
    if (link.front() == '/') {
        return baseOrigin + link;
    }

    std::string directory = base.target;
    auto qpos = directory.find('?');
    if (qpos != std::string::npos) directory = directory.substr(0, qpos);
    auto slashPos = directory.rfind('/');
    if (slashPos == std::string::npos) {
        directory = "/";
    } else {
        directory = directory.substr(0, slashPos + 1);
    }
    return baseOrigin + directory + link;
}

std::vector<std::string> extractLinks(const std::string& html)
{
    std::vector<std::string> links;
    static const std::regex hrefRegex(R"(<a\s+[^>]*href\s*=\s*["']([^"']+)["'])", std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), hrefRegex), end; it != end; ++it) {
        links.push_back((*it)[1].str());
    }
    return links;
}

std::string stripHtml(const std::string& html)
{
    return std::regex_replace(html, std::regex("<[^>]*>"), " ");
}

std::string downloadPage(const std::string& url, ssl::context& sslCtx, int redirects = 0)
{
    if (redirects > 5) {
        throw std::runtime_error("Too many redirects for URL: " + url);
    }

    UrlParts parts;
    if (!parseUrl(url, parts)) {
        throw std::runtime_error("Invalid URL: " + url);
    }

    net::io_context ioc;
    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(parts.host, parts.port);

    http::request<http::string_body> req{http::verb::get, parts.target, 11};
    req.set(http::field::host, parts.host);
    req.set(http::field::user_agent, "DiplomaSpiderBot/1.0");

    beast::flat_buffer buffer;
    http::response<http::string_body> res;

    if (parts.scheme == "https") {
        beast::ssl_stream<beast::tcp_stream> stream(ioc, sslCtx);
        if (!SSL_set_tlsext_host_name(stream.native_handle(), parts.host.c_str())) {
            throw std::runtime_error("Failed to set TLS SNI host");
        }
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);
        http::write(stream, req);
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.shutdown(ec);
        if (ec == net::error::eof) ec = {};
        if (ec) throw beast::system_error{ec};
    } else {
        beast::tcp_stream stream(ioc);
        stream.connect(results);
        http::write(stream, req);
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    auto status = static_cast<unsigned>(res.result_int());
    if (status >= 300 && status < 400 && res.base().count(http::field::location) > 0) {
        std::string location = std::string(res.base()[http::field::location]);
        std::string nextUrl = resolveUrl(url, location);
        if (nextUrl.empty()) {
            throw std::runtime_error("Redirect location is invalid: " + location);
        }
        return downloadPage(nextUrl, sslCtx, redirects + 1);
    }

    return res.body();
}

class Spider {
public:
    Spider(Database& db, int maxDepth, int threadCount)
        : db_(db), maxDepth_(maxDepth), threadCount_(threadCount), sslCtx_(ssl::context::tls_client)
    {
        sslCtx_.set_default_verify_paths();
        sslCtx_.set_verify_mode(ssl::verify_peer);
    }

    void run(const std::string& startUrl)
    {
        enqueue({startUrl, 1});

        std::vector<std::thread> workers;
        for (int i = 0; i < threadCount_; ++i) {
            workers.emplace_back([this]() { workerLoop(); });
        }

        for (auto& t : workers) {
            t.join();
        }
    }

private:
    Database& db_;
    int maxDepth_;
    int threadCount_;
    ssl::context sslCtx_;

    std::queue<Task> queue_;
    std::unordered_set<std::string> visited_;
    std::mutex queueMutex_;
    std::mutex visitedMutex_;
    std::mutex dbMutex_;
    std::condition_variable cv_;
    std::size_t activeWorkers_ = 0;
    bool finished_ = false;

    void enqueue(const Task& t)
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        queue_.push(t);
        cv_.notify_one();
    }

    bool markVisited(const std::string& url)
    {
        std::lock_guard<std::mutex> lk(visitedMutex_);
        auto [_, inserted] = visited_.insert(url);
        return inserted;
    }

    void processTask(const Task& task)
    {
        if (task.depth > maxDepth_) return;
        if (!markVisited(task.url)) return;

        try {
            std::cout << "[Spider] Downloading depth " << task.depth << ": " << task.url << "\n";
            std::string html = downloadPage(task.url, sslCtx_);
            auto freq = Indexer::countWords(stripHtml(html));

            {
                std::lock_guard<std::mutex> dbLock(dbMutex_);
                int docId = db_.saveDocument(task.url, html);
                db_.clearDocumentFrequencies(docId);
                for (const auto& [word, count] : freq) {
                    int wordId = db_.getWordId(word);
                    db_.saveFrequency(docId, wordId, count);
                }
            }

            if (task.depth < maxDepth_) {
                for (const auto& href : extractLinks(html)) {
                    std::string next = resolveUrl(task.url, href);
                    if (!next.empty()) {
                        enqueue({next, task.depth + 1});
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[Spider] Error for URL " << task.url << ": " << e.what() << "\n";
        }
    }

    void workerLoop()
    {
        while (true) {
            Task task;

            {
                std::unique_lock<std::mutex> lk(queueMutex_);
                cv_.wait(lk, [this]() { return finished_ || !queue_.empty(); });

                if (finished_ && queue_.empty()) return;

                task = queue_.front();
                queue_.pop();
                ++activeWorkers_;
            }

            processTask(task);

            {
                std::lock_guard<std::mutex> lk(queueMutex_);
                --activeWorkers_;
                if (queue_.empty() && activeWorkers_ == 0) {
                    finished_ = true;
                    cv_.notify_all();
                }
            }
        }
    }
};

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

        std::string startUrl = cfg.get("spider.start_url", cfg.get("start_url", "https://example.com"));
        int maxDepth = cfg.getInt("spider.max_depth", cfg.getInt("max_depth", cfg.getInt("spider.max_pages", 1)));
        int threads = cfg.getInt("spider.threads", cfg.getInt("threads", 4));

        if (maxDepth < 1) maxDepth = 1;
        if (threads < 1) threads = 1;

        std::cout << "Spider started from " << startUrl
                  << " with max_depth=" << maxDepth
                  << " threads=" << threads << "\n";

        Spider spider(db, maxDepth, threads);
        spider.run(startUrl);

        std::cout << "Spider finished!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
