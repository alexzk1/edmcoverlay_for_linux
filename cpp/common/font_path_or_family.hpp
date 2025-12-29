#pragma once

#include <filesystem>
#include <string>
#include <variant>

using FontPathOrFamily = std::variant<std::filesystem::path, std::string>;
