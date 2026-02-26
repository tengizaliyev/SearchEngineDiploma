#pragma once
#include <string>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>

class Config {
public:
    std::unordered_map<std::string, std::string> values;

    static std::string trim(const std::string& s) {
        std::size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
            ++start;
        }
        std::size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        return s.substr(start, end - start);
    }

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line;
        std::string section;
        while (getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }

            if (line.front() == '[' && line.back() == ']') {
                section = trim(line.substr(1, line.size() - 2));
                continue;
            }

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = trim(line.substr(0, pos));
            std::string val = trim(line.substr(pos + 1));
            if (key.empty()) continue;

            values[key] = val;
            if (!section.empty()) {
                values[section + "." + key] = val;
            }
        }
        return true;
    }

    std::string get(const std::string& key, const std::string& fallback = "") const {
        auto it = values.find(key);
        return it == values.end() ? fallback : it->second;
    }

    int getInt(const std::string& key, int fallback = 0) const {
        auto raw = get(key);
        if (raw.empty()) return fallback;
        try {
            return std::stoi(raw);
        } catch (...) {
            return fallback;
        }
    }
};
