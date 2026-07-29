#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>

#include <nlohmann/json.hpp>

struct JsonlReadStats
{
    std::size_t valid_objects{};
    std::size_t invalid_lines{};
    bool opened{};
};

using JsonlObjectCallback = std::function<void(const nlohmann::json&)>;

JsonlReadStats ReadJsonlObjects(const std::filesystem::path& path,
                                const JsonlObjectCallback& callback);
