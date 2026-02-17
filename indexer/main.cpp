#include "../include/config.hpp"
#include "../include/db.hpp"
#include "../include/indexer.hpp"

#include <iostream>
#include <regex>

std::string stripHTML(const std::string& html)
{
    return std::regex_replace(html, std::regex("<[^>]*>"), " ");
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

    auto docs = db.getDocuments();

    for (auto& doc : docs)
    {
        int docId = doc.first;
        std::string cleanText = stripHTML(doc.second);

        auto freq = Indexer::countWords(cleanText);

        for (auto& pair : freq)
        {
            int wordId = db.getWordId(pair.first);
            db.saveFrequency(docId, wordId, pair.second);
        }

        std::cout << "Indexed document ID: " << docId << "\n";
    }

    std::cout << "Indexing complete!\n";
}
