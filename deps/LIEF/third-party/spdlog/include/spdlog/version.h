// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#define SPDLOG_VER_MAJOR 1
#define SPDLOG_VER_MINOR 15
#define SPDLOG_VER_PATCH 3

#define SPDLOG_TO_VERSION(major, minor, patch) (major * 10000 + minor * 100 + patch)
#define SPDLOG_VERSION SPDLOG_TO_VERSION(SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH)
