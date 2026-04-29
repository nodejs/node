#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#define ATA_VERSION "0.10.5"

namespace ata {

inline constexpr uint32_t VERSION_MAJOR = 0;
inline constexpr uint32_t VERSION_MINOR = 10;
inline constexpr uint32_t VERSION_REVISION = 5;

inline constexpr std::string_view version() noexcept {
  return "0.10.5";
}

enum class error_code : uint8_t {
  ok = 0,
  invalid_json,
  invalid_schema,
  type_mismatch,
  required_property_missing,
  additional_property_not_allowed,
  enum_mismatch,
  const_mismatch,
  minimum_violation,
  maximum_violation,
  exclusive_minimum_violation,
  exclusive_maximum_violation,
  min_length_violation,
  max_length_violation,
  pattern_mismatch,
  format_mismatch,
  min_items_violation,
  max_items_violation,
  unique_items_violation,
  min_properties_violation,
  max_properties_violation,
  multiple_of_violation,
  all_of_failed,
  any_of_failed,
  one_of_failed,
  not_failed,
  ref_not_found,
  if_then_else_failed,
};

struct validation_error {
  error_code code;
  std::string path;
  std::string message;
  std::string expected;
  std::string actual;
};

struct validation_result {
  bool valid;
  std::vector<validation_error> errors;

  explicit operator bool() const noexcept { return valid; }
};

struct compiled_schema;

struct schema_warning {
  std::string path;
  std::string message;
};

struct schema_ref {
  std::shared_ptr<compiled_schema> impl;
  std::vector<schema_warning> warnings;

  explicit operator bool() const noexcept { return impl != nullptr; }
};

struct validate_options {
  bool all_errors = true;  // false = stop at first error (faster)
};

// Compile a JSON Schema string into an internal representation.
schema_ref compile(std::string_view schema_json);

// Validate a JSON document against a compiled schema.
validation_result validate(const schema_ref& schema, std::string_view json,
                           const validate_options& opts = {});

// Format a validation_error as a single-line prose sentence.
std::string format_prose(const validation_error& err);

// Validate a JSON document against a schema (compiles schema each time).
validation_result validate(std::string_view schema_json,
                           std::string_view json,
                           const validate_options& opts = {});

// Ultra-fast boolean validation — no error collection, no allocation.
// Input MUST have at least 64 bytes of padding after data (simdjson requirement).
// Use this when you only need true/false and can provide pre-padded input.
bool is_valid_prepadded(const schema_ref& schema, const char* data, size_t length);

// Validate raw buffer — handles padding internally via thread-local copy.
// Use this when input doesn't have simdjson padding (e.g., from V8 TypedArray).
bool is_valid_buf(const schema_ref& schema, const uint8_t* data, size_t length);

// Required padding size for is_valid_prepadded
inline constexpr size_t REQUIRED_PADDING = 64;

}  // namespace ata
