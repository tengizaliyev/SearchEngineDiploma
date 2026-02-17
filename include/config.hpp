#pragma once
#include <string>
#include <fstream>
#include <unordered_map>

class Config {
public:
    std::unordered_map<std::string, std::string> values;

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '[' || line[0] == '#')
                continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);

            values[key] = val;
        }
        return true;
    }

    std::string get(const std::string& key) {
        return values[key];
    }
};
