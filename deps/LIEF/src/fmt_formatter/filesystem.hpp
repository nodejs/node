#pragma once

#include <filesystem>
#include <string>

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

template<>
struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string> {
  auto format(const std::filesystem::path& path, format_context& ctx) const {
    return fmt::format_to(ctx.out(), "{}", path.generic_string());
  }
};
