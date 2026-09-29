#pragma once

#include <string>
#include <system_error>

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

template<>
struct fmt::formatter<std::error_code> : fmt::formatter<std::string> {
  auto format(const std::error_code& ec, format_context& ctx) const {
    return fmt::format_to(ctx.out(), "{}:{} ({})", ec.category().name(),
                          ec.value(), ec.message());
  }
};
