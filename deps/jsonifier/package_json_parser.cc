#include "node_package_json_parser.h"
#include "package_json_backend.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>

#if defined(__x86_64__) || defined(_M_X64)
#define NODE_JSONIFIER_X64 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#else
#define NODE_JSONIFIER_X64 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define NODE_JSONIFIER_ARM64 1
#else
#define NODE_JSONIFIER_ARM64 0
#endif

namespace node {
namespace modules {

namespace {

bool EndsWithObjectClose(std::string_view json) {
  const size_t last = json.find_last_not_of(" \t\n\r");
  return last != std::string_view::npos && json[last] == '}';
}

enum class JsonifierIsa { kFallback, kAvx2, kAvx512, kNeon };

#if NODE_JSONIFIER_X64

struct CpuidRegisters {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
};

CpuidRegisters QueryCpuid(uint32_t leaf, uint32_t subleaf) {
#if defined(_MSC_VER)
  int registers[4];
  __cpuidex(registers, static_cast<int>(leaf), static_cast<int>(subleaf));
  return {static_cast<uint32_t>(registers[0]),
          static_cast<uint32_t>(registers[1]),
          static_cast<uint32_t>(registers[2]),
          static_cast<uint32_t>(registers[3])};
#else
  CpuidRegisters registers{};
  __cpuid_count(leaf,
                subleaf,
                registers.eax,
                registers.ebx,
                registers.ecx,
                registers.edx);
  return registers;
#endif
}

uint64_t QueryXcr0() {
#if defined(_MSC_VER) && !defined(__clang__)
  return _xgetbv(0);
#else
  uint32_t low;
  uint32_t high;
  __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
  return (static_cast<uint64_t>(high) << 32) | low;
#endif
}

constexpr uint32_t Bit(uint32_t index) {
  return uint32_t{1} << index;
}

constexpr bool HasAll(uint32_t value, uint32_t mask) {
  return (value & mask) == mask;
}

JsonifierIsa DetectSupportedIsa() {
  if (QueryCpuid(0, 0).eax < 7 || QueryCpuid(0x80000000, 0).eax < 0x80000001) {
    return JsonifierIsa::kFallback;
  }
  constexpr uint32_t kPclmulPopcntOsxsaveAvx =
      Bit(1) | Bit(23) | Bit(27) | Bit(28);
  constexpr uint32_t kBmi1Avx2Bmi2 = Bit(3) | Bit(5) | Bit(8);
  constexpr uint32_t kLzcnt = Bit(5);
  constexpr uint64_t kXmmYmmState = 0x6;
  const CpuidRegisters leaf1 = QueryCpuid(1, 0);
  if (!HasAll(leaf1.ecx, kPclmulPopcntOsxsaveAvx)) {
    return JsonifierIsa::kFallback;
  }
  const uint64_t xcr0 = QueryXcr0();
  const CpuidRegisters leaf7 = QueryCpuid(7, 0);
  if ((xcr0 & kXmmYmmState) != kXmmYmmState ||
      !HasAll(leaf7.ebx, kBmi1Avx2Bmi2) ||
      !HasAll(QueryCpuid(0x80000001, 0).ecx, kLzcnt)) {
    return JsonifierIsa::kFallback;
  }
  constexpr uint32_t kAvx512FDqBwVl = Bit(16) | Bit(17) | Bit(30) | Bit(31);
  constexpr uint32_t kAvx512Vbmi2 = Bit(6);
  constexpr uint64_t kOpmaskZmmState = 0xE6;
  if ((xcr0 & kOpmaskZmmState) == kOpmaskZmmState &&
      HasAll(leaf7.ebx, kAvx512FDqBwVl) && HasAll(leaf7.ecx, kAvx512Vbmi2)) {
    return JsonifierIsa::kAvx512;
  }
  return JsonifierIsa::kAvx2;
}

JsonifierIsa RequestedIsaCap() {
  const char* requested = std::getenv("NODE_JSONIFIER_ISA");
  if (requested == nullptr) {
    return JsonifierIsa::kAvx512;
  }
  const std::string_view name{requested};
  if (name == "fallback") {
    return JsonifierIsa::kFallback;
  }
  if (name == "avx2") {
    return JsonifierIsa::kAvx2;
  }
  return JsonifierIsa::kAvx512;
}

JsonifierIsa DetectIsa() {
  return std::min(DetectSupportedIsa(), RequestedIsaCap());
}

#elif NODE_JSONIFIER_ARM64

JsonifierIsa DetectIsa() {
  return JsonifierIsa::kNeon;
}

#else

JsonifierIsa DetectIsa() {
  return JsonifierIsa::kFallback;
}

#endif

JsonifierIsa ActiveIsa() {
  static const JsonifierIsa isa = DetectIsa();
  return isa;
}

PackageJSONBackendParser* NewGenericParser() {
  switch (ActiveIsa()) {
#if NODE_JSONIFIER_X64
    case JsonifierIsa::kAvx512:
      return isa_avx512::NewGenericParser();
    case JsonifierIsa::kAvx2:
      return isa_avx2::NewGenericParser();
    default:
      return isa_fallback::NewGenericParser();
#elif NODE_JSONIFIER_ARM64
    default:
      return isa_neon::NewGenericParser();
#else
    default:
      return isa_fallback::NewGenericParser();
#endif
  }
}

std::string_view WalkJsonForIsa(std::string_view json,
                                json::JsonEventSink* sink) {
  switch (ActiveIsa()) {
#if NODE_JSONIFIER_X64
    case JsonifierIsa::kAvx512:
      return isa_avx512::WalkJson(json, sink);
    case JsonifierIsa::kAvx2:
      return isa_avx2::WalkJson(json, sink);
    default:
      return isa_fallback::WalkJson(json, sink);
#elif NODE_JSONIFIER_ARM64
    default:
      return isa_neon::WalkJson(json, sink);
#else
    default:
      return isa_fallback::WalkJson(json, sink);
#endif
  }
}

}  // namespace

PackageJSONBackendParser::~PackageJSONBackendParser() = default;

void PackageJSONFieldSink::SetName(std::string_view value) {
  fields_->name.emplace(value);
}

void PackageJSONFieldSink::SetMain(std::string_view value) {
  fields_->main.emplace(value);
}

void PackageJSONFieldSink::SetType(std::string_view value) {
  fields_->type.assign(value);
}

void PackageJSONFieldSink::SetExports(std::string_view value) {
  fields_->exports.emplace(value);
}

void PackageJSONFieldSink::SetImports(std::string_view value) {
  fields_->imports.emplace(value);
}

void PackageJSONFieldSink::SetScripts(std::string_view value) {
  fields_->scripts.emplace(value);
}

void PackageJSONFieldSink::SetPrivate(bool value) {
  fields_->is_private = value;
}

struct JsonifierPackageJSONParser::Impl {
  std::unique_ptr<PackageJSONBackendParser> backend{NewGenericParser()};
};

JsonifierPackageJSONParser::JsonifierPackageJSONParser()
    : impl_(std::make_unique<Impl>()) {}

JsonifierPackageJSONParser::~JsonifierPackageJSONParser() = default;

bool JsonifierPackageJSONParser::Parse(std::string_view json,
                                       PackageJSONFields* fields) {
  if (!EndsWithObjectClose(json)) {
    return false;
  }
  PackageJSONFieldSink sink{fields};
  return impl_->backend->Parse(json, &sink);
}

}  // namespace modules
}  // namespace node

namespace node {
namespace json {

JsonEventSink::~JsonEventSink() = default;

std::string_view WalkJson(std::string_view json, JsonEventSink* sink) {
  return modules::WalkJsonForIsa(json, sink);
}

}  // namespace json
}  // namespace node
