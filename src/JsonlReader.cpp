#include "JsonlReader.h"

#include <fstream>

JsonlReadStats ReadJsonlObjects(const std::filesystem::path& path,
                                const JsonlObjectCallback& callback)
{
    JsonlReadStats stats{};
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
        return stats;

    stats.opened = true;
    std::string line;
    bool first_line = true;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (first_line && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
        {
            line.erase(0, 3);
        }
        first_line = false;

        if (line.empty())
            continue;

        const auto json = nlohmann::json::parse(line, nullptr, false, false);
        if (json.is_discarded() || !json.is_object())
        {
            ++stats.invalid_lines;
            continue;
        }

        ++stats.valid_objects;
        callback(json);
    }

    return stats;
}
