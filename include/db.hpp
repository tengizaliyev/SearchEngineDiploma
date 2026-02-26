#pragma once
#include <pqxx/pqxx>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>

class Database {
private:
    pqxx::connection conn;

public:
    Database(const std::string& connStr) : conn(connStr)
    {
        std::cout << "DB Connected successfully!\n";
        ensureSchema();
    }

    void ensureSchema()
    {
        pqxx::work txn(conn);
        txn.exec(
            "CREATE TABLE IF NOT EXISTS documents ("
            "id SERIAL PRIMARY KEY, "
            "url TEXT UNIQUE, "
            "content TEXT"
            ")"
        );
        txn.exec(
            "CREATE TABLE IF NOT EXISTS words ("
            "id SERIAL PRIMARY KEY, "
            "word TEXT UNIQUE"
            ")"
        );
        txn.exec(
            "CREATE TABLE IF NOT EXISTS word_frequency ("
            "document_id INT REFERENCES documents(id) ON DELETE CASCADE, "
            "word_id INT REFERENCES words(id) ON DELETE CASCADE, "
            "count INT NOT NULL, "
            "PRIMARY KEY(document_id, word_id)"
            ")"
        );
        txn.commit();
    }

    int saveDocument(const std::string& url, const std::string& content)
    {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "INSERT INTO documents(url, content) VALUES($1,$2) "
            "ON CONFLICT (url) DO UPDATE SET content = EXCLUDED.content "
            "RETURNING id",
            url, content
        );
        txn.commit();
        std::cout << "Saved document: " << url << "\n";
        return r[0][0].as<int>();
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

    void clearDocumentFrequencies(int docId)
    {
        pqxx::work txn(conn);
        txn.exec_params(
            "DELETE FROM word_frequency WHERE document_id = $1",
            docId
        );
        txn.commit();
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

    std::vector<std::pair<std::string, int>> searchDocuments(const std::vector<std::string>& words)
    {
        std::vector<std::pair<std::string, int>> results;
        if (words.empty()) return results;

        pqxx::work txn(conn);

        std::ostringstream sql;
        sql
            << "SELECT d.url, SUM(wf.count) AS relevance "
            "FROM documents d "
            "JOIN word_frequency wf ON d.id = wf.document_id "
            "JOIN words w ON w.id = wf.word_id "
            "WHERE w.word IN (";

        for (std::size_t i = 0; i < words.size(); ++i) {
            if (i > 0) sql << ",";
            sql << txn.quote(words[i]);
        }

        sql
            << ") "
            << "GROUP BY d.id, d.url "
            << "HAVING COUNT(DISTINCT w.word) = " << words.size() << " "
            << "ORDER BY relevance DESC "
            << "LIMIT 10";

        pqxx::result r = txn.exec(sql.str());

        txn.commit();

        for (auto row : r) {
            results.push_back({row[0].as<std::string>(), row[1].as<int>()});
        }

        return results;
    }
};
