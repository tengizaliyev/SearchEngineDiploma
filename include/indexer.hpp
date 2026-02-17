#pragma once
#include <string>
#include <sstream>
#include <unordered_map>
#include <algorithm>

class Indexer {
public:
    static std::unordered_map<std::string,int> countWords(const std::string& text) {
        std::unordered_map<std::string,int> freq;
        std::stringstream ss(text);
        std::string word;

        while (ss >> word) {
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);

            if (word.size() < 3) continue;
            freq[word]++;
        }

        return freq;
    }
};
