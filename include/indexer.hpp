#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

class Indexer {
public:
    static std::unordered_map<std::string, int> countWords(const std::string& text) {
        std::unordered_map<std::string, int> freq;
        std::string word;
        word.reserve(32);

        auto commitWord = [&]() {
            if (word.size() >= 3 && word.size() <= 32) {
                ++freq[word];
            }
            word.clear();
        };

        for (unsigned char ch : text) {
            if (std::isalnum(ch)) {
                word.push_back(static_cast<char>(std::tolower(ch)));
            } else if (!word.empty()) {
                commitWord();
            }
        }

        if (!word.empty()) {
            commitWord();
        }

        return freq;
    }
};
