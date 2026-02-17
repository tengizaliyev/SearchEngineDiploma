#pragma once
#include <pqxx/pqxx>
#include <string>
#include <iostream>
#include <vector>

class Database {
private:
    pqxx::connection conn;

public:
    Database(const std::string& connStr) : conn(connStr)
    {
        std::cout << "DB Connected successfully!\n";
    }

    void saveDocument(const std::string& url, const std::string& content)
    {
        pqxx::work txn(conn);
        txn.exec_params(
            "INSERT INTO documents(url, content) VALUES($1,$2) "
            "ON CONFLICT (url) DO NOTHING",
            url, content
        );
        txn.commit();
        std::cout << "Saved document: " << url << "\n";
    }

    std::vector<std::pair<int, std::string>> getDocuments()
    {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec("SELECT id, content FROM documents");
        txn.commit();

        std::vector<std::pair<int, std::string>> docs;
        for (auto row : r)
            docs.push_back({row[0].as<int>(), row[1].as<std::string>()});

        return docs;
    }

    int getWordId(const std::string& word)
    {
        pqxx::work txn(conn);

        pqxx::result r = txn.exec_params(
            "INSERT INTO words(word) VALUES($1) "
            "ON CONFLICT (word) DO UPDATE SET word=EXCLUDED.word "
            "RETURNING id",
            word
        );

        int id = r[0][0].as<int>();
        txn.commit();
        return id;
    }

    void saveFrequency(int docId, int wordId, int count)
    {
        pqxx::work txn(conn);

        txn.exec_params(
            "INSERT INTO word_frequency(document_id, word_id, count) "
            "VALUES($1,$2,$3) "
            "ON CONFLICT (document_id, word_id) DO UPDATE SET count=$3",
            docId, wordId, count
        );

        txn.commit();
    }

    std::vector<std::string> searchWord(const std::string& word)
    {
        pqxx::work txn(conn);

        pqxx::result r = txn.exec_params(
            "SELECT d.url "
            "FROM documents d "
            "JOIN word_frequency wf ON d.id = wf.document_id "
            "JOIN words w ON w.id = wf.word_id "
            "WHERE w.word = $1 "
            "ORDER BY wf.count DESC "
            "LIMIT 10",
            word
        );

        txn.commit();

        std::vector<std::string> results;
        for (auto row : r)
            results.push_back(row[0].as<std::string>());

        return results;
    }
};
