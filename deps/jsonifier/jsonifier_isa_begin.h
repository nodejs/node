#ifndef DEPS_JSONIFIER_JSONIFIER_ISA_BEGIN_H_
#define DEPS_JSONIFIER_JSONIFIER_ISA_BEGIN_H_

#include "package_json_backend.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if NODE_JSONIFIER_ISA_AVX512
#define JSONIFIER_LZCNT 1
#define JSONIFIER_POPCNT 1
#define JSONIFIER_BMI 1
#define JSONIFIER_CLMUL 1
#define JSONIFIER_AVX 1
#define JSONIFIER_AVX2 1
#define JSONIFIER_AVX512 1
#define JSONIFIER_NEON 0
#define JSONIFIER_SVE2 0
#define JSONIFIER_CPU_ALIGNMENT 64
#define jsonifier jsonifier_avx512
#define NODE_JSONIFIER_ISA_NS isa_avx512
#elif NODE_JSONIFIER_ISA_AVX2
#define JSONIFIER_LZCNT 1
#define JSONIFIER_POPCNT 1
#define JSONIFIER_BMI 1
#define JSONIFIER_CLMUL 1
#define JSONIFIER_AVX 1
#define JSONIFIER_AVX2 1
#define JSONIFIER_AVX512 0
#define JSONIFIER_NEON 0
#define JSONIFIER_SVE2 0
#define JSONIFIER_CPU_ALIGNMENT 32
#define jsonifier jsonifier_avx2
#define NODE_JSONIFIER_ISA_NS isa_avx2
#elif NODE_JSONIFIER_ISA_NEON
#define JSONIFIER_LZCNT 0
#define JSONIFIER_POPCNT 0
#define JSONIFIER_BMI 0
#define JSONIFIER_CLMUL 0
#define JSONIFIER_AVX 0
#define JSONIFIER_AVX2 0
#define JSONIFIER_AVX512 0
#define JSONIFIER_NEON 1
#define JSONIFIER_SVE2 0
#define JSONIFIER_CPU_ALIGNMENT 16
#define jsonifier jsonifier_neon
#define NODE_JSONIFIER_ISA_NS isa_neon
#else
#define JSONIFIER_LZCNT 0
#define JSONIFIER_POPCNT 0
#define JSONIFIER_BMI 0
#define JSONIFIER_CLMUL 0
#define JSONIFIER_AVX 0
#define JSONIFIER_AVX2 0
#define JSONIFIER_AVX512 0
#define JSONIFIER_NEON 0
#define JSONIFIER_SVE2 0
#define JSONIFIER_CPU_ALIGNMENT 16
#define jsonifier jsonifier_fallback
#define NODE_JSONIFIER_ISA_NS isa_fallback
#endif

#include <jsonifier>

#endif  // DEPS_JSONIFIER_JSONIFIER_ISA_BEGIN_H_
