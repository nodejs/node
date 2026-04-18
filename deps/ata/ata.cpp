/* auto-generated on 2026-04-18 10:54:35 +0100. Do not edit! */
/* begin file src/ata.cpp */
#include "ata.h"

// mimalloc: faster new/delete for small allocations.
#if __has_include(<mimalloc-new-delete.h>)
#include <mimalloc-new-delete.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#ifndef ATA_NO_RE2
#include <re2/re2.h>
#endif
#include <set>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#else
#include <unistd.h>
#endif

// MSVC implementation by Pavel P (https://gist.github.com/pps83/3210a2f980fd02bb2ba2e5a1fc4a2ef0)
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#ifndef __builtin_popcount
#define __builtin_popcount __popcnt
#endif
#endif // defined(_MSC_VER) && !defined(__clang__)

#include "simdjson.h"

// --- Fast format validators (no std::regex) ---

static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }
static bool is_hex(char c) {
  return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool fast_check_email(std::string_view s) {
  auto at = s.find('@');
  if (at == std::string_view::npos || at == 0 || at == s.size() - 1)
    return false;
  auto dot = s.find('.', at + 1);
  if (dot == std::string_view::npos || dot == at + 1 ||
      dot == s.size() - 1)
    return false;
  // Check TLD has at least 2 chars
  return (s.size() - dot - 1) >= 2;
}

static bool fast_check_date(std::string_view s) {
  // YYYY-MM-DD with range validation
  if (s.size() != 10 || !is_digit(s[0]) || !is_digit(s[1]) ||
      !is_digit(s[2]) || !is_digit(s[3]) || s[4] != '-' ||
      !is_digit(s[5]) || !is_digit(s[6]) || s[7] != '-' ||
      !is_digit(s[8]) || !is_digit(s[9]))
    return false;
  int month = (s[5] - '0') * 10 + (s[6] - '0');
  int day = (s[8] - '0') * 10 + (s[9] - '0');
  return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

static bool fast_check_time(std::string_view s) {
  // HH:MM:SS[.frac][Z|+HH:MM]
  if (s.size() < 8) return false;
  if (!is_digit(s[0]) || !is_digit(s[1]) || s[2] != ':' ||
      !is_digit(s[3]) || !is_digit(s[4]) || s[5] != ':' ||
      !is_digit(s[6]) || !is_digit(s[7]))
    return false;
  return true;
}

static bool fast_check_datetime(std::string_view s) {
  if (s.size() < 19) return false;
  if (!fast_check_date(s.substr(0, 10))) return false;
  if (s[10] != 'T' && s[10] != 't' && s[10] != ' ') return false;
  return fast_check_time(s.substr(11));
}

static bool fast_check_ipv4(std::string_view s) {
  int parts = 0, val = 0, digits = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == '.') {
      if (digits == 0 || val > 255) return false;
      ++parts;
      val = 0;
      digits = 0;
    } else if (is_digit(s[i])) {
      val = val * 10 + (s[i] - '0');
      ++digits;
      if (digits > 3) return false;
    } else {
      return false;
    }
  }
  return parts == 4;
}

static bool fast_check_uri(std::string_view s) {
  if (s.size() < 3) return false;
  // Must start with alpha, then scheme chars, then ':'
  if (!is_alpha(s[0])) return false;
  size_t i = 1;
  while (i < s.size() && (is_alnum(s[i]) || s[i] == '+' || s[i] == '-' ||
                           s[i] == '.'))
    ++i;
  return i < s.size() && s[i] == ':' && i + 1 < s.size();
}

static bool fast_check_uuid(std::string_view s) {
  // 8-4-4-4-12
  if (s.size() != 36) return false;
  for (size_t i = 0; i < 36; ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (s[i] != '-') return false;
    } else {
      if (!is_hex(s[i])) return false;
    }
  }
  return true;
}

static bool fast_check_hostname(std::string_view s) {
  if (s.empty() || s.size() > 253) return false;
  size_t label_len = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '.') {
      if (label_len == 0) return false;
      label_len = 0;
    } else if (is_alnum(s[i]) || s[i] == '-') {
      ++label_len;
      if (label_len > 63) return false;
    } else {
      return false;
    }
  }
  return label_len > 0;
}

// Check format by pre-resolved numeric ID — no string comparisons.
static bool check_format_by_id(std::string_view sv, uint8_t fid) {
  switch (fid) {
    case 0: return fast_check_email(sv);
    case 1: return fast_check_date(sv);
    case 2: return fast_check_datetime(sv);
    case 3: return fast_check_time(sv);
    case 4: return fast_check_ipv4(sv);
    case 5: return sv.find(':') != std::string_view::npos;
    case 6: return fast_check_uri(sv);
    case 7: return fast_check_uuid(sv);
    case 8: return fast_check_hostname(sv);
    default: return true;  // unknown formats pass
  }
}

namespace ata {

using namespace simdjson;

// Canonical JSON: sort object keys for semantic equality comparison
static std::string canonical_json(dom::element el) {
  switch (el.type()) {
    case dom::element_type::OBJECT: {
      dom::object obj; el.get(obj);
      std::vector<std::pair<std::string_view, dom::element>> entries;
      for (auto [k, v] : obj) entries.push_back({k, v});
      std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
      std::string r = "{";
      for (size_t i = 0; i < entries.size(); ++i) {
        if (i) r += ',';
        r += '"';
        r += entries[i].first;
        r += "\":";
        r += canonical_json(entries[i].second);
      }
      r += '}';
      return r;
    }
    case dom::element_type::ARRAY: {
      dom::array arr; el.get(arr);
      std::string r = "[";
      bool first = true;
      for (auto v : arr) {
        if (!first) r += ',';
        first = false;
        r += canonical_json(v);
      }
      r += ']';
      return r;
    }
    default:
      return std::string(minify(el));
  }
}

// JSON Schema type enum — avoids string comparisons on the hot path.
enum class json_type : uint8_t {
  string, number, integer, boolean, null_value, object, array
};

static json_type json_type_from_sv(std::string_view s) {
  if (s == "string")  return json_type::string;
  if (s == "number")  return json_type::number;
  if (s == "integer") return json_type::integer;
  if (s == "boolean") return json_type::boolean;
  if (s == "null")    return json_type::null_value;
  if (s == "object")  return json_type::object;
  if (s == "array")   return json_type::array;
  return json_type::string; // fallback
}

static const char* json_type_name(json_type t) {
  switch (t) {
    case json_type::string:     return "string";
    case json_type::number:     return "number";
    case json_type::integer:    return "integer";
    case json_type::boolean:    return "boolean";
    case json_type::null_value: return "null";
    case json_type::object:     return "object";
    case json_type::array:      return "array";
  }
  return "unknown";
}

// Bitmask for O(1) type checking: one bit per json_type value.
static uint8_t json_type_bit(json_type t) { return 1u << static_cast<uint8_t>(t); }

// Map dom::element_type to a json_type bitmask (number matches integer too).
static uint8_t element_type_mask(dom::element_type t) {
  switch (t) {
    case dom::element_type::STRING:     return json_type_bit(json_type::string);
    case dom::element_type::INT64:
    case dom::element_type::UINT64:     return json_type_bit(json_type::integer) | json_type_bit(json_type::number);
    case dom::element_type::DOUBLE:     return json_type_bit(json_type::number);
    case dom::element_type::BOOL:       return json_type_bit(json_type::boolean);
    case dom::element_type::NULL_VALUE: return json_type_bit(json_type::null_value);
    case dom::element_type::ARRAY:      return json_type_bit(json_type::array);
    case dom::element_type::OBJECT:     return json_type_bit(json_type::object);
  }
  return 0;
}

// Resolve format string to numeric ID at compile time.
static uint8_t format_id_from_string(const std::string& f) {
  if (f == "email")          return 0;
  if (f == "date")           return 1;
  if (f == "date-time")      return 2;
  if (f == "time")           return 3;
  if (f == "ipv4")           return 4;
  if (f == "ipv6")           return 5;
  if (f == "uri" || f == "uri-reference") return 6;
  if (f == "uuid")           return 7;
  if (f == "hostname")       return 8;
  return 255;
}

// Forward declarations
struct schema_node;
using schema_node_ptr = std::shared_ptr<schema_node>;

struct schema_node {
  // type constraint — bitmask for O(1) type checking
  uint8_t type_mask = 0;  // bit per json_type value

  // numeric
  std::optional<double> minimum;
  std::optional<double> maximum;
  std::optional<double> exclusive_minimum;
  std::optional<double> exclusive_maximum;
  std::optional<double> multiple_of;

  // string
  std::optional<uint64_t> min_length;
  std::optional<uint64_t> max_length;
  std::optional<std::string> pattern;
#ifndef ATA_NO_RE2
  std::shared_ptr<re2::RE2> compiled_pattern;  // cached compiled regex (RE2)
#endif

  // array
  std::optional<uint64_t> min_items;
  std::optional<uint64_t> max_items;
  bool unique_items = false;
  schema_node_ptr items_schema;
  std::vector<schema_node_ptr> prefix_items;
  schema_node_ptr contains_schema;
  std::optional<uint64_t> min_contains;
  std::optional<uint64_t> max_contains;

  // object
  std::unordered_map<std::string, schema_node_ptr> properties;
  std::vector<std::string> required;
  std::optional<bool> additional_properties_bool;
  schema_node_ptr additional_properties_schema;
  std::optional<uint64_t> min_properties;
  std::optional<uint64_t> max_properties;
  schema_node_ptr property_names_schema;
  std::unordered_map<std::string, std::vector<std::string>> dependent_required;
  std::unordered_map<std::string, schema_node_ptr> dependent_schemas;

  // patternProperties — each entry: (pattern_string, schema, compiled_regex)
  struct pattern_prop {
    std::string pattern;
    schema_node_ptr schema;
#ifndef ATA_NO_RE2
    std::shared_ptr<re2::RE2> compiled;
#endif
  };
  std::vector<pattern_prop> pattern_properties;

  // enum / const
  std::vector<std::string> enum_values_minified;  // pre-minified enum values
  std::optional<std::string> const_value_raw;  // raw JSON value string

  // format
  std::optional<std::string> format;
  uint8_t format_id = 255;  // pre-resolved format ID (255 = unknown/pass)

  // composition
  std::vector<schema_node_ptr> all_of;
  std::vector<schema_node_ptr> any_of;
  std::vector<schema_node_ptr> one_of;
  schema_node_ptr not_schema;

  // conditional
  schema_node_ptr if_schema;
  schema_node_ptr then_schema;
  schema_node_ptr else_schema;

  // $ref
  std::string ref;
  std::string dynamic_ref;    // $dynamicRef value (e.g. "#items")
  std::string id;             // $id — resource boundary marker

  // $defs — stored on node for pointer navigation
  std::unordered_map<std::string, schema_node_ptr> defs;

  // boolean schema
  std::optional<bool> boolean_schema;
};

// --- Codegen: flat bytecode plan ---
namespace cg {
enum class op : uint8_t {
  END=0, EXPECT_OBJECT, EXPECT_ARRAY, EXPECT_STRING, EXPECT_NUMBER,
  EXPECT_INTEGER, EXPECT_BOOLEAN, EXPECT_NULL, EXPECT_TYPE_MULTI,
  CHECK_MINIMUM, CHECK_MAXIMUM, CHECK_EX_MINIMUM, CHECK_EX_MAXIMUM,
  CHECK_MULTIPLE_OF, CHECK_MIN_LENGTH, CHECK_MAX_LENGTH, CHECK_PATTERN,
  CHECK_FORMAT, CHECK_MIN_ITEMS, CHECK_MAX_ITEMS, CHECK_UNIQUE_ITEMS,
  ARRAY_ITEMS, CHECK_REQUIRED, CHECK_MIN_PROPS, CHECK_MAX_PROPS,
  OBJ_PROPS_START, OBJ_PROP, OBJ_PROPS_END, CHECK_NO_ADDITIONAL,
  CHECK_ENUM_STR, CHECK_ENUM, CHECK_CONST, COMPOSITION,
};
struct ins { op o; uint32_t a=0, b=0; };
struct plan {
  std::vector<ins> code;
  std::vector<double> doubles;
  std::vector<std::string> strings;
#ifndef ATA_NO_RE2
  std::vector<std::shared_ptr<re2::RE2>> regexes;
#endif
  std::vector<std::vector<std::string>> enum_sets;
  std::vector<uint8_t> type_masks;
  std::vector<uint8_t> format_ids;
  std::vector<std::vector<ins>> subs;
};
}  // namespace cg

// --- On-Demand validation plan ---
// Grouped checks per value type. Each value consumed exactly once.
// Built from schema_node at compile time, used by od_exec_plan at runtime.
struct od_plan {
  uint8_t type_mask = 0;

  // Numeric — bitmask for which checks to run + flat array of bounds
  enum num_flag : uint8_t {
    HAS_MIN = 1, HAS_MAX = 2, HAS_EX_MIN = 4, HAS_EX_MAX = 8, HAS_MUL = 16
  };
  uint8_t num_flags = 0;
  double num_min = 0, num_max = 0, num_ex_min = 0, num_ex_max = 0, num_mul = 0;

  // String — single value.get(sv) then all checks
  std::optional<uint64_t> min_length, max_length;
#ifndef ATA_NO_RE2
  re2::RE2* pattern = nullptr;        // borrowed pointer from schema_node
#endif
  uint8_t format_id = 255;            // 255 = no format check

  // Object — single iterate with merged required+property lookup
  struct prop_entry {
    std::string key;
    int required_idx = -1;            // bit index for required tracking, or -1
    std::shared_ptr<od_plan> sub;     // property sub-plan, or nullptr
  };
  struct obj_plan {
    std::vector<prop_entry> entries;  // merged required + properties — single scan
    size_t required_count = 0;
    bool no_additional = false;
    std::optional<uint64_t> min_props, max_props;
  };
  std::shared_ptr<obj_plan> object;

  // Array — single iterate: items + count
  struct arr_plan {
    std::shared_ptr<od_plan> items;
    std::optional<uint64_t> min_items, max_items;
  };
  std::shared_ptr<arr_plan> array;

  // If false, schema uses unsupported features — must fall back to DOM path.
  bool supported = true;
};

using od_plan_ptr = std::shared_ptr<od_plan>;

struct compiled_schema {
  schema_node_ptr root;
  std::unordered_map<std::string, schema_node_ptr> defs;
  std::string raw_schema;
  std::string compile_error;  // non-empty if compilation failed
  dom::parser parser;          // used only at compile time
  cg::plan gen_plan;           // codegen validation plan
  bool use_ondemand = false;   // true if codegen plan supports On Demand
  od_plan_ptr od;              // On-Demand execution plan

  // anchor resolution
  std::unordered_map<std::string, schema_node_ptr> anchors;
  std::unordered_map<std::string,
    std::unordered_map<std::string, schema_node_ptr>> resource_dynamic_anchors;
  bool has_dynamic_refs = false;
  std::string current_resource_id;  // compile-time only

  // compile-time warnings (misplaced keywords, etc.)
  std::vector<schema_warning> warnings;
  std::string compile_path;  // current JSON pointer during compilation
};

// Thread-local persistent parsers — reused across all validate calls on the
// same thread.  Keeps internal buffers hot in cache and avoids re-allocation.
static dom::parser& tl_dom_parser() {
  thread_local dom::parser p;
  return p;
}
static dom::parser& tl_dom_key_parser() {
  thread_local dom::parser p;
  return p;
}
static simdjson::ondemand::parser& tl_od_parser() {
  thread_local simdjson::ondemand::parser p;
  return p;
};

// --- Schema compilation ---

static schema_node_ptr compile_node(dom::element el,
                                    compiled_schema& ctx);

static schema_node_ptr compile_node(dom::element el,
                                    compiled_schema& ctx) {
  auto node = std::make_shared<schema_node>();

  // Boolean schema
  if (el.is<bool>()) {
    bool bval;
    el.get(bval);
    node->boolean_schema = bval;
    return node;
  }

  if (!el.is<dom::object>()) {
    return node;
  }

  dom::object obj;
  el.get(obj);

  // $ref
  dom::element ref_el;
  if (obj["$ref"].get(ref_el) == SUCCESS) {
    std::string_view ref_sv;
    if (ref_el.get(ref_sv) == SUCCESS) {
      node->ref = std::string(ref_sv);
    }
  }

  // $id — must come before $anchor/$dynamicAnchor so current_resource_id is set
  std::string prev_resource = ctx.current_resource_id;
  {
    dom::element id_el;
    if (obj["$id"].get(id_el) == SUCCESS) {
      std::string_view sv;
      if (id_el.get(sv) == SUCCESS) {
        node->id = std::string(sv);
        ctx.current_resource_id = node->id;
        ctx.defs[node->id] = node;
      }
    }
  }

  // $anchor — register in flat anchor map
  {
    dom::element anchor_el;
    if (obj["$anchor"].get(anchor_el) == SUCCESS) {
      std::string_view sv;
      if (anchor_el.get(sv) == SUCCESS) {
        ctx.anchors[std::string(sv)] = node;
      }
    }
  }

  // $dynamicAnchor — register in both flat anchors and per-resource map
  {
    dom::element da_el;
    if (obj["$dynamicAnchor"].get(da_el) == SUCCESS) {
      std::string_view sv;
      if (da_el.get(sv) == SUCCESS) {
        std::string name(sv);
        ctx.anchors[name] = node;
        ctx.resource_dynamic_anchors[ctx.current_resource_id][name] = node;
      }
    }
  }

  // $dynamicRef
  {
    dom::element dr_el;
    if (obj["$dynamicRef"].get(dr_el) == SUCCESS) {
      std::string_view sv;
      if (dr_el.get(sv) == SUCCESS) {
        std::string dr_val(sv);
        // If the $dynamicRef starts with "#" (fragment-only) and we're inside
        // a non-root resource, qualify it with the current resource ID so
        // validation can resolve it correctly.
        if (!dr_val.empty() && dr_val[0] == '#' &&
            !ctx.current_resource_id.empty()) {
          dr_val = ctx.current_resource_id + dr_val;
        }
        node->dynamic_ref = dr_val;
        ctx.has_dynamic_refs = true;
      }
    }
  }

  // type
  dom::element type_el;
  if (obj["type"].get(type_el) == SUCCESS) {
    if (type_el.is<std::string_view>()) {
      std::string_view sv;
      type_el.get(sv);
      node->type_mask |= json_type_bit(json_type_from_sv(sv));
    } else if (type_el.is<dom::array>()) {
      dom::array type_arr; type_el.get(type_arr); for (auto t : type_arr) {
        std::string_view sv;
        if (t.get(sv) == SUCCESS) {
          node->type_mask |= json_type_bit(json_type_from_sv(sv));
        }
      }
    }
  }

  // numeric constraints
  dom::element num_el;
  if (obj["minimum"].get(num_el) == SUCCESS) {
    double v;
    if (num_el.get(v) == SUCCESS) node->minimum = v;
  }
  if (obj["maximum"].get(num_el) == SUCCESS) {
    double v;
    if (num_el.get(v) == SUCCESS) node->maximum = v;
  }
  if (obj["exclusiveMinimum"].get(num_el) == SUCCESS) {
    double v;
    if (num_el.get(v) == SUCCESS) node->exclusive_minimum = v;
  }
  if (obj["exclusiveMaximum"].get(num_el) == SUCCESS) {
    double v;
    if (num_el.get(v) == SUCCESS) node->exclusive_maximum = v;
  }
  if (obj["multipleOf"].get(num_el) == SUCCESS) {
    double v;
    if (num_el.get(v) == SUCCESS) node->multiple_of = v;
  }

  // string constraints
  dom::element str_el;
  if (obj["minLength"].get(str_el) == SUCCESS) {
    uint64_t v;
    if (str_el.get(v) == SUCCESS) node->min_length = v;
  }
  if (obj["maxLength"].get(str_el) == SUCCESS) {
    uint64_t v;
    if (str_el.get(v) == SUCCESS) node->max_length = v;
  }
  if (obj["pattern"].get(str_el) == SUCCESS) {
    std::string_view sv;
    if (str_el.get(sv) == SUCCESS) {
      node->pattern = std::string(sv);
#ifdef ATA_NO_RE2
      ctx.compile_error = "pattern keyword requires RE2 support (built with ATA_NO_RE2)";
      return node;
#else
      auto re = std::make_shared<re2::RE2>(node->pattern.value());
      if (re->ok()) {
        node->compiled_pattern = std::move(re);
      }
#endif
    }
  }

  // array constraints
  if (obj["minItems"].get(str_el) == SUCCESS) {
    uint64_t v;
    if (str_el.get(v) == SUCCESS) node->min_items = v;
  }
  if (obj["maxItems"].get(str_el) == SUCCESS) {
    uint64_t v;
    if (str_el.get(v) == SUCCESS) node->max_items = v;
  }
  dom::element ui_el;
  if (obj["uniqueItems"].get(ui_el) == SUCCESS) {
    bool v;
    if (ui_el.get(v) == SUCCESS) node->unique_items = v;
  }
  // prefixItems (Draft 2020-12)
  dom::element pi_el;
  if (obj["prefixItems"].get(pi_el) == SUCCESS && pi_el.is<dom::array>()) {
    dom::array pi_arr; pi_el.get(pi_arr); for (auto item : pi_arr) {
      node->prefix_items.push_back(compile_node(item, ctx));
    }
  }

  dom::element items_el;
  if (obj["items"].get(items_el) == SUCCESS) {
    node->items_schema = compile_node(items_el, ctx);
  }

  // contains
  dom::element contains_el;
  if (obj["contains"].get(contains_el) == SUCCESS) {
    node->contains_schema = compile_node(contains_el, ctx);
  }
  dom::element mc_el;
  if (obj["minContains"].get(mc_el) == SUCCESS) {
    uint64_t v;
    if (mc_el.get(v) == SUCCESS) node->min_contains = v;
  }
  if (obj["maxContains"].get(mc_el) == SUCCESS) {
    uint64_t v;
    if (mc_el.get(v) == SUCCESS) node->max_contains = v;
  }

  // object constraints
  dom::element props_el;
  if (obj["properties"].get(props_el) == SUCCESS && props_el.is<dom::object>()) {
    dom::object props_obj; props_el.get(props_obj); for (auto [key, val] : props_obj) {
      node->properties[std::string(key)] = compile_node(val, ctx);
    }
  }

  dom::element req_el;
  if (obj["required"].get(req_el) == SUCCESS && req_el.is<dom::array>()) {
    dom::array req_arr; req_el.get(req_arr); for (auto r : req_arr) {
      std::string_view sv;
      if (r.get(sv) == SUCCESS) {
        node->required.emplace_back(sv);
      }
    }
  }

  dom::element ap_el;
  if (obj["additionalProperties"].get(ap_el) == SUCCESS) {
    if (ap_el.is<bool>()) {
      bool ap_bool; ap_el.get(ap_bool); node->additional_properties_bool = ap_bool;
    } else {
      node->additional_properties_schema = compile_node(ap_el, ctx);
    }
  }

  if (obj["minProperties"].get(str_el) == SUCCESS) {
    uint64_t v;
    if (str_el.get(v) == SUCCESS) node->min_properties = v;
  }
  if (obj["maxProperties"].get(str_el) == SUCCESS) {
    uint64_t v;
    if (str_el.get(v) == SUCCESS) node->max_properties = v;
  }

  // propertyNames
  dom::element pn_el;
  if (obj["propertyNames"].get(pn_el) == SUCCESS) {
    node->property_names_schema = compile_node(pn_el, ctx);
  }

  // dependentRequired
  dom::element dr_el;
  if (obj["dependentRequired"].get(dr_el) == SUCCESS &&
      dr_el.is<dom::object>()) {
    dom::object dr_obj; dr_el.get(dr_obj); for (auto [key, val] : dr_obj) {
      std::vector<std::string> deps;
      if (val.is<dom::array>()) {
        dom::array val_arr; val.get(val_arr); for (auto d : val_arr) {
          std::string_view sv;
          if (d.get(sv) == SUCCESS) deps.emplace_back(sv);
        }
      }
      node->dependent_required[std::string(key)] = std::move(deps);
    }
  }

  // dependentSchemas
  dom::element ds_el;
  if (obj["dependentSchemas"].get(ds_el) == SUCCESS &&
      ds_el.is<dom::object>()) {
    dom::object ds_obj; ds_el.get(ds_obj); for (auto [key, val] : ds_obj) {
      node->dependent_schemas[std::string(key)] = compile_node(val, ctx);
    }
  }

  // patternProperties — compile regex at schema compile time
  dom::element pp_el;
  if (obj["patternProperties"].get(pp_el) == SUCCESS &&
      pp_el.is<dom::object>()) {
#ifdef ATA_NO_RE2
    ctx.compile_error = "patternProperties keyword requires RE2 support (built with ATA_NO_RE2)";
    return node;
#else
    dom::object pp_obj; pp_el.get(pp_obj);
    for (auto [key, val] : pp_obj) {
      schema_node::pattern_prop pp;
      pp.pattern = std::string(key);
      pp.schema = compile_node(val, ctx);
      auto re = std::make_shared<re2::RE2>(pp.pattern);
      if (re->ok()) {
        pp.compiled = std::move(re);
      }
      node->pattern_properties.push_back(std::move(pp));
    }
#endif
  }

  // format
  dom::element fmt_el;
  if (obj["format"].get(fmt_el) == SUCCESS) {
    std::string_view sv;
    if (fmt_el.get(sv) == SUCCESS) {
      node->format = std::string(sv);
      node->format_id = format_id_from_string(node->format.value());
    }
  }

  // enum — pre-minify each value at compile time
  dom::element enum_el;
  if (obj["enum"].get(enum_el) == SUCCESS) {
    if (enum_el.is<dom::array>()) {
      dom::array enum_arr; enum_el.get(enum_arr); for (auto e : enum_arr) {
        node->enum_values_minified.push_back(canonical_json(e));
      }
    }
  }

  // const
  dom::element const_el;
  if (obj["const"].get(const_el) == SUCCESS) {
    node->const_value_raw = canonical_json(const_el);
  }

  // composition
  dom::element comp_el;
  if (obj["allOf"].get(comp_el) == SUCCESS && comp_el.is<dom::array>()) {
    dom::array comp_arr; comp_el.get(comp_arr);
    for (auto s : comp_arr) {
      node->all_of.push_back(compile_node(s, ctx));
    }
  }
  if (obj["anyOf"].get(comp_el) == SUCCESS && comp_el.is<dom::array>()) {
    dom::array comp_arr2; comp_el.get(comp_arr2);
    for (auto s : comp_arr2) {
      node->any_of.push_back(compile_node(s, ctx));
    }
  }
  if (obj["oneOf"].get(comp_el) == SUCCESS && comp_el.is<dom::array>()) {
    dom::array comp_arr3; comp_el.get(comp_arr3);
    for (auto s : comp_arr3) {
      node->one_of.push_back(compile_node(s, ctx));
    }
  }
  dom::element not_el;
  if (obj["not"].get(not_el) == SUCCESS) {
    node->not_schema = compile_node(not_el, ctx);
  }

  // conditional
  dom::element if_el;
  if (obj["if"].get(if_el) == SUCCESS) {
    node->if_schema = compile_node(if_el, ctx);
  }
  dom::element then_el;
  if (obj["then"].get(then_el) == SUCCESS) {
    node->then_schema = compile_node(then_el, ctx);
  }
  dom::element else_el;
  if (obj["else"].get(else_el) == SUCCESS) {
    node->else_schema = compile_node(else_el, ctx);
  }

  // $defs / definitions
  dom::element defs_el;
  if (obj["$defs"].get(defs_el) == SUCCESS && defs_el.is<dom::object>()) {
    dom::object defs_obj; defs_el.get(defs_obj); for (auto [key, val] : defs_obj) {
      std::string def_path = "#/$defs/" + std::string(key);
      auto compiled = compile_node(val, ctx);
      ctx.defs[def_path] = compiled;
      node->defs[std::string(key)] = compiled;
    }
  }
  if (obj["definitions"].get(defs_el) == SUCCESS &&
      defs_el.is<dom::object>()) {
    dom::object defs_obj; defs_el.get(defs_obj); for (auto [key, val] : defs_obj) {
      std::string def_path = "#/definitions/" + std::string(key);
      auto compiled = compile_node(val, ctx);
      ctx.defs[def_path] = compiled;
      node->defs[std::string(key)] = compiled;
    }
  }

  // Warn about keywords used at the wrong type level.
  // Only check when an explicit "type" is declared (type_mask != 0).
  if (node->type_mask != 0) {
    const uint8_t array_bit = json_type_bit(json_type::array);
    const uint8_t string_bit = json_type_bit(json_type::string);
    const uint8_t number_bits = json_type_bit(json_type::number) |
                                json_type_bit(json_type::integer);
    const uint8_t object_bit = json_type_bit(json_type::object);

    auto warn = [&](const char* keyword, const char* expected_type) {
      ctx.warnings.push_back({
        ctx.compile_path,
        std::string(keyword) + " has no effect on type \"" +
        (node->type_mask & json_type_bit(json_type::string) ? "string" :
         node->type_mask & json_type_bit(json_type::boolean) ? "boolean" :
         node->type_mask & json_type_bit(json_type::number) ? "number" :
         node->type_mask & object_bit ? "object" :
         node->type_mask & array_bit ? "array" : "unknown") +
        "\", only applies to " + expected_type
      });
    };

    // Array keywords on non-array type
    if (!(node->type_mask & array_bit)) {
      if (node->min_items.has_value()) warn("minItems", "array");
      if (node->max_items.has_value()) warn("maxItems", "array");
      if (node->unique_items) warn("uniqueItems", "array");
      if (!node->prefix_items.empty()) warn("prefixItems", "array");
      if (node->items_schema) warn("items", "array");
      if (node->contains_schema) warn("contains", "array");
    }

    // String keywords on non-string type
    if (!(node->type_mask & string_bit)) {
      if (node->min_length.has_value()) warn("minLength", "string");
      if (node->max_length.has_value()) warn("maxLength", "string");
      if (node->pattern.has_value()) warn("pattern", "string");
    }

    // Numeric keywords on non-numeric type
    if (!(node->type_mask & number_bits)) {
      if (node->minimum.has_value()) warn("minimum", "number");
      if (node->maximum.has_value()) warn("maximum", "number");
      if (node->exclusive_minimum.has_value()) warn("exclusiveMinimum", "number");
      if (node->exclusive_maximum.has_value()) warn("exclusiveMaximum", "number");
      if (node->multiple_of.has_value()) warn("multipleOf", "number");
    }

    // Object keywords on non-object type
    if (!(node->type_mask & object_bit)) {
      if (!node->properties.empty()) warn("properties", "object");
      if (!node->required.empty()) warn("required", "object");
    }
  }

  ctx.current_resource_id = prev_resource;
  return node;
}

// --- Validation ---

using dynamic_scope_t = std::vector<const std::unordered_map<std::string, schema_node_ptr>*>;

// Decode a single JSON Pointer segment (percent-decode, then ~1->/, ~0->~)
static std::string decode_pointer_segment(const std::string& seg) {
  std::string pct;
  for (size_t i = 0; i < seg.size(); ++i) {
    if (seg[i] == '%' && i + 2 < seg.size()) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
      };
      int hv = hex(seg[i+1]), lv = hex(seg[i+2]);
      if (hv >= 0 && lv >= 0) {
        pct += static_cast<char>(hv * 16 + lv);
        i += 2;
      } else {
        pct += seg[i];
      }
    } else {
      pct += seg[i];
    }
  }
  std::string out;
  for (size_t i = 0; i < pct.size(); ++i) {
    if (pct[i] == '~' && i + 1 < pct.size()) {
      if (pct[i + 1] == '1') { out += '/'; ++i; }
      else if (pct[i + 1] == '0') { out += '~'; ++i; }
      else out += pct[i];
    } else {
      out += pct[i];
    }
  }
  return out;
}

// Walk a JSON Pointer (without leading #) within a given schema node.
// Returns the resolved node, or nullptr if not found.
static schema_node_ptr walk_json_pointer(const schema_node_ptr& root_node,
                                          const std::string& pointer) {
  if (pointer.empty()) return root_node;

  std::vector<std::string> segments;
  size_t spos = 0;
  // pointer starts with "/" — skip leading slash
  if (!pointer.empty() && pointer[0] == '/') spos = 1;
  while (spos <= pointer.size()) {
    size_t snext = pointer.find('/', spos);
    segments.push_back(decode_pointer_segment(
        pointer.substr(spos, snext == std::string::npos ? snext : snext - spos)));
    spos = (snext == std::string::npos) ? pointer.size() + 1 : snext + 1;
  }

  schema_node_ptr current = root_node;
  for (size_t si = 0; si < segments.size() && current; ++si) {
    const auto& key = segments[si];
    if (key == "properties" && si + 1 < segments.size()) {
      const auto& prop_name = segments[++si];
      auto pit = current->properties.find(prop_name);
      if (pit != current->properties.end()) { current = pit->second; }
      else { return nullptr; }
    } else if (key == "items" && current->items_schema) {
      current = current->items_schema;
    } else if (key == "$defs" || key == "definitions") {
      if (si + 1 < segments.size()) {
        const auto& def_name = segments[++si];
        auto dit = current->defs.find(def_name);
        if (dit != current->defs.end()) { current = dit->second; }
        else { return nullptr; }
      } else { return nullptr; }
    } else if (key == "allOf" || key == "anyOf" || key == "oneOf") {
      if (si + 1 < segments.size()) {
        size_t idx = std::stoul(segments[++si]);
        auto& vec = (key == "allOf") ? current->all_of
                  : (key == "anyOf") ? current->any_of
                  : current->one_of;
        if (idx < vec.size()) { current = vec[idx]; }
        else { return nullptr; }
      } else { return nullptr; }
    } else if (key == "not" && current->not_schema) {
      current = current->not_schema;
    } else if (key == "if" && current->if_schema) {
      current = current->if_schema;
    } else if (key == "then" && current->then_schema) {
      current = current->then_schema;
    } else if (key == "else" && current->else_schema) {
      current = current->else_schema;
    } else if (key == "additionalProperties" &&
               current->additional_properties_schema) {
      current = current->additional_properties_schema;
    } else if (key == "prefixItems") {
      if (si + 1 < segments.size()) {
        size_t idx = std::stoul(segments[++si]);
        if (idx < current->prefix_items.size()) { current = current->prefix_items[idx]; }
        else { return nullptr; }
      } else { return nullptr; }
    } else if (key == "contains" && current->contains_schema) {
      current = current->contains_schema;
    } else if (key == "propertyNames" && current->property_names_schema) {
      current = current->property_names_schema;
    } else {
      return nullptr;
    }
  }
  return current;
}

// Find an anchor (non-pointer fragment) within a specific resource node by
// searching its sub-tree.  Used for resolving "base#anchor" references.
static schema_node_ptr find_anchor_in_resource(const compiled_schema& ctx,
                                                const std::string& resource_id,
                                                const std::string& anchor_name) {
  // Look up in per-resource dynamic anchors first
  auto rit = ctx.resource_dynamic_anchors.find(resource_id);
  if (rit != ctx.resource_dynamic_anchors.end()) {
    auto ait = rit->second.find(anchor_name);
    if (ait != rit->second.end()) return ait->second;
  }
  // Fallback to flat anchors (which includes $anchor entries)
  auto ait = ctx.anchors.find(anchor_name);
  if (ait != ctx.anchors.end()) return ait->second;
  return nullptr;
}

static void validate_node(const schema_node_ptr& node,
                           dom::element value,
                           const std::string& path,
                           const compiled_schema& ctx,
                           std::vector<validation_error>& errors,
                           bool all_errors = true,
                           dynamic_scope_t* dynamic_scope = nullptr);

// Fast boolean-only tree walker — no error collection, no string allocation.
// Uses [[likely]]/[[unlikely]] hints. Returns true if valid.
static bool validate_fast(const schema_node_ptr& node,
                           dom::element value,
                           const compiled_schema& ctx);

// Macro for early termination
#define ATA_CHECK_EARLY() if (!all_errors && !errors.empty()) return

using et = dom::element_type;


// Use string_view to avoid allocations in hot path
static std::string_view type_of_sv(dom::element el) {
  switch (el.type()) {
    case et::STRING:    return "string";
    case et::INT64:
    case et::UINT64:    return "integer";
    case et::DOUBLE:    return "number";
    case et::BOOL:      return "boolean";
    case et::NULL_VALUE:return "null";
    case et::ARRAY:     return "array";
    case et::OBJECT:    return "object";
  }
  return "unknown";
}


// O(1) type check: test element's type bits against the schema's type_mask.
static bool type_matches_mask(dom::element el, uint8_t type_mask) {
  return (element_type_mask(el.type()) & type_mask) != 0;
}

static double to_double(dom::element el) {
  switch (el.type()) {
    case et::DOUBLE:  { double v; el.get(v); return v; }
    case et::INT64:   { int64_t v; el.get(v); return static_cast<double>(v); }
    case et::UINT64:  { uint64_t v; el.get(v); return static_cast<double>(v); }
    default: return 0;
  }
}

// Count UTF-8 codepoints — branchless: count non-continuation bytes
static uint64_t utf8_length(std::string_view s) {
  uint64_t count = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    // Continuation bytes are 10xxxxxx (0x80-0xBF)
    // Non-continuation bytes start codepoints
    count += ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80);
  }
  return count;
}

// Recursion depth guard — prevents stack overflow on self-referencing schemas
struct DepthGuard {
  static thread_local int depth;
  bool overflow;
  DepthGuard() : overflow(++depth > 100) {}
  ~DepthGuard() { --depth; }
};
thread_local int DepthGuard::depth = 0;

static void validate_node(const schema_node_ptr& node,
                           dom::element value,
                           const std::string& path,
                           const compiled_schema& ctx,
                           std::vector<validation_error>& errors,
                           bool all_errors,
                           dynamic_scope_t* dynamic_scope) {
  if (!node) return;

  DepthGuard guard;
  if (guard.overflow) return;

  // Boolean schema
  if (node->boolean_schema.has_value()) {
    if (!node->boolean_schema.value()) {
      errors.push_back({error_code::type_mismatch, path,
                        "schema is false, no value is valid"});
    }
    return;
  }

  // Dynamic scope tracking: push this resource's dynamic anchors
  bool pushed_scope = false;
  if (dynamic_scope && !node->id.empty()) {
    auto it = ctx.resource_dynamic_anchors.find(node->id);
    if (it != ctx.resource_dynamic_anchors.end()) {
      dynamic_scope->push_back(&it->second);
      pushed_scope = true;
    }
  }

  // $ref — Draft 2020-12: $ref is not a short-circuit, sibling keywords still apply
  bool ref_resolved = false;
  if (!node->ref.empty()) {
    // Self-reference: "#"
    if (node->ref == "#" && ctx.root) {
      validate_node(ctx.root, value, path, ctx, errors, all_errors, dynamic_scope);
      ref_resolved = true;
    }
    // Check for "base#fragment" pattern (e.g. "first#/$defs/stuff", "tree.json")
    if (!ref_resolved) {
      std::string base_uri;
      std::string fragment;
      size_t hash_pos = node->ref.find('#');
      if (hash_pos != std::string::npos) {
        base_uri = node->ref.substr(0, hash_pos);
        fragment = node->ref.substr(hash_pos + 1);
      } else {
        base_uri = node->ref;
      }

      // Helper: push base resource's dynamic anchors to scope, validate, pop
      auto validate_with_resource_scope = [&](const schema_node_ptr& target,
                                               const std::string& resource_id) {
        bool scope_pushed = false;
        if (dynamic_scope && !resource_id.empty()) {
          auto rit = ctx.resource_dynamic_anchors.find(resource_id);
          if (rit != ctx.resource_dynamic_anchors.end()) {
            dynamic_scope->push_back(&rit->second);
            scope_pushed = true;
          }
        }
        validate_node(target, value, path, ctx, errors, all_errors, dynamic_scope);
        if (scope_pushed) dynamic_scope->pop_back();
      };

      if (!base_uri.empty()) {
        // Resolve base URI to a resource via defs
        auto it = ctx.defs.find(base_uri);
        if (it != ctx.defs.end()) {
          schema_node_ptr target = it->second;
          if (!fragment.empty()) {
            if (fragment[0] == '/') {
              // JSON Pointer within the resource
              auto resolved = walk_json_pointer(target, fragment);
              if (resolved) {
                validate_with_resource_scope(resolved, base_uri);
                ref_resolved = true;
              }
            } else {
              // Anchor lookup within the resource
              auto resolved = find_anchor_in_resource(ctx, base_uri, fragment);
              if (resolved) {
                validate_with_resource_scope(resolved, base_uri);
                ref_resolved = true;
              }
            }
          } else {
            // No fragment, just the base resource (it pushes its own scope)
            validate_node(target, value, path, ctx, errors, all_errors, dynamic_scope);
            ref_resolved = true;
          }
        }
      } else if (!fragment.empty()) {
        // "#fragment" — no base URI
        if (fragment[0] == '/') {
          // JSON Pointer from root
          auto resolved = walk_json_pointer(ctx.root, fragment);
          if (resolved) {
            validate_node(resolved, value, path, ctx, errors, all_errors, dynamic_scope);
            ref_resolved = true;
          }
        } else {
          // Anchor lookup
          auto ait = ctx.anchors.find(fragment);
          if (ait != ctx.anchors.end()) {
            validate_node(ait->second, value, path, ctx, errors, all_errors, dynamic_scope);
            ref_resolved = true;
          }
        }
      }
    }
    // Fallback: try defs map directly (handles bare $id references like "list")
    if (!ref_resolved) {
      auto it = ctx.defs.find(node->ref);
      if (it != ctx.defs.end()) {
        validate_node(it->second, value, path, ctx, errors, all_errors, dynamic_scope);
        ref_resolved = true;
      }
    }
    // Fallback: relative URI resolution — match ref against defs keys by suffix
    if (!ref_resolved && !node->ref.empty() && node->ref[0] != '#') {
      std::string suffix = "/" + node->ref;
      for (const auto& [key, def_node] : ctx.defs) {
        if (key.size() >= suffix.size() &&
            key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
          validate_node(def_node, value, path, ctx, errors, all_errors, dynamic_scope);
          ref_resolved = true;
          break;
        }
      }
    }
    if (!ref_resolved) {
      errors.push_back({error_code::ref_not_found, path,
                        "cannot resolve $ref: " + node->ref});
    }
  }

  // $dynamicRef — Draft 2020-12 dynamic scope resolution
  if (!node->dynamic_ref.empty()) {
    bool dref_resolved = false;

    // Parse the $dynamicRef value into base URI and fragment
    std::string dr_base;
    std::string dr_fragment;
    {
      size_t hash_pos = node->dynamic_ref.find('#');
      if (hash_pos != std::string::npos) {
        dr_base = node->dynamic_ref.substr(0, hash_pos);
        dr_fragment = node->dynamic_ref.substr(hash_pos + 1);
      } else {
        dr_base = node->dynamic_ref;
      }
    }

    // Helper: push base resource's dynamic anchors to scope temporarily
    auto push_resource_scope = [&](const std::string& resource_id) -> bool {
      if (dynamic_scope && !resource_id.empty()) {
        auto rit = ctx.resource_dynamic_anchors.find(resource_id);
        if (rit != ctx.resource_dynamic_anchors.end()) {
          dynamic_scope->push_back(&rit->second);
          return true;
        }
      }
      return false;
    };

    // If fragment is a JSON pointer (starts with /), resolve like $ref
    if (!dr_fragment.empty() && dr_fragment[0] == '/') {
      schema_node_ptr base_node = dr_base.empty() ? ctx.root : nullptr;
      if (!dr_base.empty()) {
        auto it = ctx.defs.find(dr_base);
        if (it != ctx.defs.end()) base_node = it->second;
      }
      if (base_node) {
        auto resolved = walk_json_pointer(base_node, dr_fragment);
        if (resolved) {
          bool dr_scope_pushed = push_resource_scope(dr_base);
          validate_node(resolved, value, path, ctx, errors, all_errors, dynamic_scope);
          if (dr_scope_pushed) dynamic_scope->pop_back();
          dref_resolved = true;
        }
      }
    }

    // If fragment is an anchor name (not a JSON pointer)
    if (!dref_resolved && !dr_fragment.empty() && dr_fragment[0] != '/') {
      std::string anchor_name = dr_fragment;

      // Initial resolution: find the anchor
      schema_node_ptr target = nullptr;

      if (!dr_base.empty()) {
        // Resolve base URI first, then find anchor in that resource
        auto it = ctx.defs.find(dr_base);
        if (it != ctx.defs.end()) {
          target = find_anchor_in_resource(ctx, dr_base, anchor_name);
        }
      } else {
        // No base URI — look up in flat anchors map
        auto ait = ctx.anchors.find(anchor_name);
        if (ait != ctx.anchors.end()) {
          target = ait->second;
        }
      }

      if (target) {
        // Check if the initially resolved target is itself a $dynamicAnchor
        // (the "bookend" requirement). Only do dynamic scope walk if the
        // initial target's resource has a $dynamicAnchor with this name.
        bool is_dynamic_at_initial = false;
        if (!dr_base.empty()) {
          // We resolved via a specific base URI
          auto rit = ctx.resource_dynamic_anchors.find(dr_base);
          if (rit != ctx.resource_dynamic_anchors.end() &&
              rit->second.count(anchor_name)) {
            is_dynamic_at_initial = true;
          }
        } else {
          // No base URI — check if ANY resource has this as $dynamicAnchor
          // and the target matches (i.e., the initially resolved node IS a
          // $dynamicAnchor node)
          for (const auto& [rid, rmap] : ctx.resource_dynamic_anchors) {
            auto ait2 = rmap.find(anchor_name);
            if (ait2 != rmap.end() && ait2->second == target) {
              is_dynamic_at_initial = true;
              break;
            }
          }
        }

        // Dynamic scope walk: find first override in dynamic scope
        if (is_dynamic_at_initial && dynamic_scope) {
          for (size_t i = 0; i < dynamic_scope->size(); ++i) {
            auto dit = (*dynamic_scope)[i]->find(anchor_name);
            if (dit != (*dynamic_scope)[i]->end()) {
              target = dit->second;
              break;
            }
          }
        }

        bool dr_scope_pushed = push_resource_scope(dr_base);
        validate_node(target, value, path, ctx, errors, all_errors, dynamic_scope);
        if (dr_scope_pushed) dynamic_scope->pop_back();
        dref_resolved = true;
      }
    }

    // Bare $dynamicRef without fragment (unusual, but handle it)
    if (!dref_resolved && dr_fragment.empty() && !dr_base.empty()) {
      auto it = ctx.defs.find(dr_base);
      if (it != ctx.defs.end()) {
        validate_node(it->second, value, path, ctx, errors, all_errors, dynamic_scope);
        dref_resolved = true;
      }
    }

    if (!dref_resolved) {
      errors.push_back({error_code::ref_not_found, path,
                        "cannot resolve $dynamicRef: " + node->dynamic_ref});
    }
  }

  // type
  if (node->type_mask) {
    if (!type_matches_mask(value, node->type_mask)) {
      std::string expected;
      for (int b = 0; b < 7; ++b) {
        if (node->type_mask & (1u << b)) {
          if (!expected.empty()) expected += ", ";
          expected += json_type_name(static_cast<json_type>(b));
        }
      }
      errors.push_back({error_code::type_mismatch, path,
                        "expected type " + expected + ", got " + std::string(type_of_sv(value))});
      ATA_CHECK_EARLY();
    }
  }

  // enum — use pre-minified values (no re-parsing)
  if (!node->enum_values_minified.empty()) {
    std::string val_str = canonical_json(value);
    bool found = false;
    for (const auto& ev : node->enum_values_minified) {
      if (ev == val_str) {
        found = true;
        break;
      }
    }
    if (!found) {
      errors.push_back({error_code::enum_mismatch, path,
                        "value not in enum"});
    }
  }

  // const
  if (node->const_value_raw.has_value()) {
    std::string val_str = canonical_json(value);
    if (val_str != node->const_value_raw.value()) {
      errors.push_back({error_code::const_mismatch, path,
                        "value does not match const"});
      ATA_CHECK_EARLY();
    }
  }

  ATA_CHECK_EARLY();
  // Numeric validations
  auto vtype = value.type();
  if (vtype == et::INT64 || vtype == et::UINT64 || vtype == et::DOUBLE) {
    double v = to_double(value);
    if (node->minimum.has_value() && v < node->minimum.value()) {
      errors.push_back({error_code::minimum_violation, path,
                        "value " + std::to_string(v) + " < minimum " +
                            std::to_string(node->minimum.value())});
    }
    if (node->maximum.has_value() && v > node->maximum.value()) {
      errors.push_back({error_code::maximum_violation, path,
                        "value " + std::to_string(v) + " > maximum " +
                            std::to_string(node->maximum.value())});
    }
    if (node->exclusive_minimum.has_value() &&
        v <= node->exclusive_minimum.value()) {
      errors.push_back({error_code::exclusive_minimum_violation, path,
                        "value must be > " +
                            std::to_string(node->exclusive_minimum.value())});
    }
    if (node->exclusive_maximum.has_value() &&
        v >= node->exclusive_maximum.value()) {
      errors.push_back({error_code::exclusive_maximum_violation, path,
                        "value must be < " +
                            std::to_string(node->exclusive_maximum.value())});
    }
    if (node->multiple_of.has_value()) {
      double divisor = node->multiple_of.value();
      double rem = std::fmod(v, divisor);
      // Use relative tolerance for floating point comparison
      if (std::abs(rem) > 1e-8 && std::abs(rem - divisor) > 1e-8) {
        errors.push_back({error_code::multiple_of_violation, path,
                          "value not a multiple of " +
                              std::to_string(node->multiple_of.value())});
      }
    }
  }

  // String validations
  if (vtype == et::STRING) {
    std::string_view sv;
    value.get(sv);
    uint64_t len = utf8_length(sv);

    if (node->min_length.has_value() && len < node->min_length.value()) {
      errors.push_back({error_code::min_length_violation, path,
                        "string length " + std::to_string(len) +
                            " < minLength " +
                            std::to_string(node->min_length.value())});
    }
    if (node->max_length.has_value() && len > node->max_length.value()) {
      errors.push_back({error_code::max_length_violation, path,
                        "string length " + std::to_string(len) +
                            " > maxLength " +
                            std::to_string(node->max_length.value())});
    }
#ifndef ATA_NO_RE2
    if (node->compiled_pattern) {
      if (!re2::RE2::PartialMatch(re2::StringPiece(sv.data(), sv.size()), *node->compiled_pattern)) {
        errors.push_back({error_code::pattern_mismatch, path,
                          "string does not match pattern: " +
                              node->pattern.value()});
      }
    }
#endif

    if (node->format.has_value()) {
      if (!check_format_by_id(sv, node->format_id)) {
        errors.push_back({error_code::format_mismatch, path,
                          "string does not match format: " +
                              node->format.value()});
      }
    }
  }

  // Array validations
  if (vtype == et::ARRAY) {
    dom::array arr; value.get(arr);
    uint64_t arr_size = arr.size();
    if(arr_size == 0xFFFFFF) [[unlikely]]	{
      // Fallback for large arrays where size() saturates — count manually to avoid overflow
      arr_size = 0;
      for ([[maybe_unused]] auto _ : arr) ++arr_size;
    }

    if (node->min_items.has_value() && arr_size < node->min_items.value()) {
      errors.push_back({error_code::min_items_violation, path,
                        "array has " + std::to_string(arr_size) +
                            " items, minimum " +
                            std::to_string(node->min_items.value())});
    }
    if (node->max_items.has_value() && arr_size > node->max_items.value()) {
      errors.push_back({error_code::max_items_violation, path,
                        "array has " + std::to_string(arr_size) +
                            " items, maximum " +
                            std::to_string(node->max_items.value())});
    }

    if (node->unique_items) {
      bool has_dup = false;
      // Fast path: check if all items are the same simple type
      auto first_it = arr.begin();
      if (first_it != arr.end()) {
        auto first_type = (*first_it).type();
        bool all_same = true;
        for (auto item : arr) { if (item.type() != first_type) { all_same = false; break; } }
        if (all_same && first_type == et::STRING) {
          std::set<std::string_view> seen;
          for (auto item : arr) {
            std::string_view sv; item.get(sv);
            if (!seen.insert(sv).second) { has_dup = true; break; }
          }
        } else if (all_same && (first_type == et::INT64 || first_type == et::UINT64 || first_type == et::DOUBLE)) {
          std::set<double> seen;
          for (auto item : arr) {
            if (!seen.insert(to_double(item)).second) { has_dup = true; break; }
          }
        } else {
          std::set<std::string> seen;
          for (auto item : arr) {
            if (!seen.insert(canonical_json(item)).second) { has_dup = true; break; }
          }
        }
      }
      if (has_dup) {
        errors.push_back({error_code::unique_items_violation, path,
                          "array contains duplicate items"});
      }
    }

    // prefixItems + items (Draft 2020-12 semantics)
    {
      uint64_t idx = 0;
      for (auto item : arr) {
        if (idx < node->prefix_items.size()) {
          validate_node(node->prefix_items[idx], item,
                        path + "/" + std::to_string(idx), ctx, errors, all_errors, dynamic_scope);
        } else if (node->items_schema) {
          validate_node(node->items_schema, item,
                        path + "/" + std::to_string(idx), ctx, errors, all_errors, dynamic_scope);
        }
        ++idx;
      }
    }

    // contains / minContains / maxContains
    if (node->contains_schema) {
      uint64_t match_count = 0;
      for (auto item : arr) {
        if (validate_fast(node->contains_schema, item, ctx)) ++match_count;
      }
      uint64_t min_c = node->min_contains.value_or(1);
      uint64_t max_c = node->max_contains.value_or(arr_size);
      if (match_count < min_c) {
        errors.push_back({error_code::min_items_violation, path,
                          "contains: " + std::to_string(match_count) +
                              " matches, minimum " + std::to_string(min_c)});
      }
      if (match_count > max_c) {
        errors.push_back({error_code::max_items_violation, path,
                          "contains: " + std::to_string(match_count) +
                              " matches, maximum " + std::to_string(max_c)});
      }
    }
  }

  // Object validations
  if (vtype == et::OBJECT) {
    dom::object obj; value.get(obj);

    if (node->min_properties.has_value() || node->max_properties.has_value()) {
      uint64_t prop_count = 0;
      for ([[maybe_unused]] auto _ : obj) ++prop_count;
      if (node->min_properties.has_value() &&
          prop_count < node->min_properties.value()) {
        errors.push_back({error_code::min_properties_violation, path,
                          "object has " + std::to_string(prop_count) +
                              " properties, minimum " +
                              std::to_string(node->min_properties.value())});
      }
      if (node->max_properties.has_value() &&
          prop_count > node->max_properties.value()) {
        errors.push_back({error_code::max_properties_violation, path,
                          "object has " + std::to_string(prop_count) +
                              " properties, maximum " +
                              std::to_string(node->max_properties.value())});
      }
    }

    // required
    for (const auto& req : node->required) {
      dom::element dummy;
      if (obj[req].get(dummy) != SUCCESS) {
        errors.push_back({error_code::required_property_missing, path,
                          "missing required property: " + req});
      }
    }

    // properties + patternProperties + additionalProperties
    for (auto [key, val] : obj) {
      std::string key_str(key);
      bool matched = false;

      // Check properties
      auto it = node->properties.find(key_str);
      if (it != node->properties.end()) {
        validate_node(it->second, val, path + "/" + key_str, ctx, errors, all_errors, dynamic_scope);
        matched = true;
      }

      // Check patternProperties (use cached compiled regex)
      for (const auto& pp : node->pattern_properties) {
#ifndef ATA_NO_RE2
        if (pp.compiled && re2::RE2::PartialMatch(key_str, *pp.compiled)) {
          validate_node(pp.schema, val, path + "/" + key_str, ctx, errors, all_errors, dynamic_scope);
          matched = true;
        }
#endif
      }

      // additionalProperties (only if not matched by properties or patternProperties)
      if (!matched) {
        if (node->additional_properties_bool.has_value() &&
            !node->additional_properties_bool.value()) {
          errors.push_back(
              {error_code::additional_property_not_allowed, path,
               "additional property not allowed: " + key_str});
        } else if (node->additional_properties_schema) {
          validate_node(node->additional_properties_schema, val,
                        path + "/" + key_str, ctx, errors, all_errors, dynamic_scope);
        }
      }
    }
    // propertyNames — validate key as string directly when possible
    if (node->property_names_schema) {
      auto pn = node->property_names_schema;
      bool string_only = pn->ref.empty() && pn->all_of.empty() &&
          pn->any_of.empty() && pn->one_of.empty() && !pn->not_schema &&
          !pn->if_schema && pn->enum_values_minified.empty() &&
          !pn->const_value_raw.has_value();
      if (string_only) {
        // Fast path: validate string constraints on key directly
        for (auto [key, val] : obj) {
          std::string_view key_sv(key);
          if (pn->type_mask && !(pn->type_mask & json_type_bit(json_type::string))) {
            errors.push_back({error_code::type_mismatch, path,
                              "propertyNames: key is string but schema requires different type"});
            continue;
          }
          uint64_t len = utf8_length(key_sv);
          if (pn->min_length.has_value() && len < pn->min_length.value()) {
            errors.push_back({error_code::min_length_violation, path,
                              "propertyNames: key too short: " + std::string(key_sv)});
          }
          if (pn->max_length.has_value() && len > pn->max_length.value()) {
            errors.push_back({error_code::max_length_violation, path,
                              "propertyNames: key too long: " + std::string(key_sv)});
          }
#ifndef ATA_NO_RE2
          if (pn->compiled_pattern) {
            if (!re2::RE2::PartialMatch(re2::StringPiece(key_sv.data(), key_sv.size()), *pn->compiled_pattern)) {
              errors.push_back({error_code::pattern_mismatch, path,
                                "propertyNames: key does not match pattern: " + std::string(key_sv)});
            }
          }
#endif
          if (pn->format.has_value() && !check_format_by_id(key_sv, pn->format_id)) {
            errors.push_back({error_code::format_mismatch, path,
                              "propertyNames: key does not match format: " + std::string(key_sv)});
          }
        }
      } else {
        // Fallback: parse key as JSON string element
        for (auto [key, val] : obj) {
          std::string key_json = "\"" + std::string(key) + "\"";
          auto key_result = tl_dom_key_parser().parse(key_json);
          if (!key_result.error()) {
            validate_node(pn, key_result.value_unsafe(), path, ctx, errors, all_errors, dynamic_scope);
          }
        }
      }
    }

    // dependentRequired
    for (const auto& [prop, deps] : node->dependent_required) {
      dom::element dummy;
      if (obj[prop].get(dummy) == SUCCESS) {
        for (const auto& dep : deps) {
          dom::element dep_dummy;
          if (obj[dep].get(dep_dummy) != SUCCESS) {
            errors.push_back({error_code::required_property_missing, path,
                              "property '" + prop + "' requires '" + dep +
                                  "' to be present"});
          }
        }
      }
    }

    // dependentSchemas
    for (const auto& [prop, schema] : node->dependent_schemas) {
      dom::element dummy;
      if (obj[prop].get(dummy) == SUCCESS) {
        validate_node(schema, value, path, ctx, errors, all_errors, dynamic_scope);
      }
    }
  }

  // allOf
  if (!node->all_of.empty()) {
    for (const auto& sub : node->all_of) {
      std::vector<validation_error> sub_errors;
      validate_node(sub, value, path, ctx, sub_errors, all_errors, dynamic_scope);
      if (!sub_errors.empty()) {
        errors.push_back({error_code::all_of_failed, path,
                          "allOf subschema failed"});
        errors.insert(errors.end(), sub_errors.begin(), sub_errors.end());
      }
    }
  }

  // anyOf
  if (!node->any_of.empty()) {
    bool any_valid = false;
    for (const auto& sub : node->any_of) {
      std::vector<validation_error> sub_errors;
      validate_node(sub, value, path, ctx, sub_errors, all_errors, dynamic_scope);
      if (sub_errors.empty()) {
        any_valid = true;
        break;
      }
    }
    if (!any_valid) {
      errors.push_back({error_code::any_of_failed, path,
                        "no anyOf subschema matched"});
    }
  }

  // oneOf
  if (!node->one_of.empty()) {
    int match_count = 0;
    for (const auto& sub : node->one_of) {
      std::vector<validation_error> sub_errors;
      validate_node(sub, value, path, ctx, sub_errors, all_errors, dynamic_scope);
      if (sub_errors.empty()) ++match_count;
    }
    if (match_count != 1) {
      errors.push_back({error_code::one_of_failed, path,
                        "expected exactly one oneOf match, got " +
                            std::to_string(match_count)});
    }
  }

  // not
  if (node->not_schema) {
    std::vector<validation_error> sub_errors;
    validate_node(node->not_schema, value, path, ctx, sub_errors, all_errors, dynamic_scope);
    if (sub_errors.empty()) {
      errors.push_back({error_code::not_failed, path,
                        "value should not match 'not' schema"});
    }
  }

  // if/then/else
  if (node->if_schema) {
    std::vector<validation_error> if_errors;
    validate_node(node->if_schema, value, path, ctx, if_errors, all_errors, dynamic_scope);
    if (if_errors.empty()) {
      // if passed → validate then
      if (node->then_schema) {
        validate_node(node->then_schema, value, path, ctx, errors, all_errors, dynamic_scope);
      }
    } else {
      // if failed → validate else
      if (node->else_schema) {
        validate_node(node->else_schema, value, path, ctx, errors, all_errors, dynamic_scope);
      }
    }
  }

  if (pushed_scope) dynamic_scope->pop_back();
}

// Fast boolean-only tree walker — stripped of all error collection.
// No std::string allocation, no path tracking, no error messages.
// Returns true if valid. Uses [[likely]]/[[unlikely]] branch hints.
static bool validate_fast(const schema_node_ptr& node,
                           dom::element value,
                           const compiled_schema& ctx) {
  if (!node) [[unlikely]] return true;

  DepthGuard guard;
  if (guard.overflow) [[unlikely]] return true;

  if (node->boolean_schema.has_value()) [[unlikely]]
    return node->boolean_schema.value();

  // $dynamicRef — bail to tree walker
  if (!node->dynamic_ref.empty()) [[unlikely]] return false;

  // $ref
  if (!node->ref.empty()) [[unlikely]] {
    auto it = ctx.defs.find(node->ref);
    if (it != ctx.defs.end()) {
      if (!validate_fast(it->second, value, ctx)) return false;
    } else if (node->ref.size() > 1 && node->ref[0] == '#' && node->ref[1] != '/') {
      auto ait = ctx.anchors.find(node->ref.substr(1));
      if (ait != ctx.anchors.end()) {
        if (!validate_fast(ait->second, value, ctx)) return false;
      } else {
        return false;
      }
    } else if (node->ref == "#" && ctx.root) {
      if (!validate_fast(ctx.root, value, ctx)) return false;
    } else {
      return false;
    }
  }

  // type
  if (node->type_mask) {
    if (!type_matches_mask(value, node->type_mask)) [[unlikely]] return false;
  }

  // enum
  if (!node->enum_values_minified.empty()) {
    auto val_str = canonical_json(value);
    bool found = false;
    for (const auto& ev : node->enum_values_minified) {
      if (ev == val_str) { found = true; break; }
    }
    if (!found) [[unlikely]] return false;
  }

  // const
  if (node->const_value_raw.has_value()) {
    if (canonical_json(value) != node->const_value_raw.value()) [[unlikely]] return false;
  }

  auto vtype = value.type();

  // Numeric
  if (vtype == et::INT64 || vtype == et::UINT64 || vtype == et::DOUBLE) {
    double v = to_double(value);
    if (node->minimum.has_value() && v < node->minimum.value()) return false;
    if (node->maximum.has_value() && v > node->maximum.value()) return false;
    if (node->exclusive_minimum.has_value() && v <= node->exclusive_minimum.value()) return false;
    if (node->exclusive_maximum.has_value() && v >= node->exclusive_maximum.value()) return false;
    if (node->multiple_of.has_value()) {
      double rem = std::fmod(v, node->multiple_of.value());
      if (std::abs(rem) > 1e-8 && std::abs(rem - node->multiple_of.value()) > 1e-8) return false;
    }
  }

  // String
  if (vtype == et::STRING) {
    std::string_view sv;
    value.get(sv);
    uint64_t len = utf8_length(sv);
    if (node->min_length.has_value() && len < node->min_length.value()) return false;
    if (node->max_length.has_value() && len > node->max_length.value()) return false;
#ifndef ATA_NO_RE2
    if (node->compiled_pattern) {
      if (!re2::RE2::PartialMatch(re2::StringPiece(sv.data(), sv.size()), *node->compiled_pattern))
        return false;
    }
#endif
    if (node->format.has_value() && !check_format_by_id(sv, node->format_id)) return false;
  }

  // Array
  if (vtype == et::ARRAY) {
    dom::array arr; value.get(arr);
    uint64_t arr_size = arr.size();
    if(arr_size == 0xFFFFFF) [[unlikely]]	{
      // Fallback for large arrays where size() saturates — count manually to avoid overflow
      arr_size = 0;
      for ([[maybe_unused]] auto _ : arr) ++arr_size;
    }

    if (node->min_items.has_value() && arr_size < node->min_items.value()) return false;
    if (node->max_items.has_value() && arr_size > node->max_items.value()) return false;

    if (node->unique_items) {
      auto first_it = arr.begin();
      if (first_it != arr.end()) {
        auto first_type = (*first_it).type();
        bool all_same = true;
        for (auto item : arr) { if (item.type() != first_type) { all_same = false; break; } }
        if (all_same && first_type == et::STRING) {
          std::set<std::string_view> seen;
          for (auto item : arr) { std::string_view sv; item.get(sv); if (!seen.insert(sv).second) return false; }
        } else if (all_same && (first_type == et::INT64 || first_type == et::UINT64 || first_type == et::DOUBLE)) {
          std::set<double> seen;
          for (auto item : arr) { if (!seen.insert(to_double(item)).second) return false; }
        } else {
          std::set<std::string> seen;
          for (auto item : arr) { if (!seen.insert(canonical_json(item)).second) return false; }
        }
      }
    }

    { uint64_t idx = 0;
      for (auto item : arr) {
        if (idx < node->prefix_items.size()) {
          if (!validate_fast(node->prefix_items[idx], item, ctx)) return false;
        } else if (node->items_schema) {
          if (!validate_fast(node->items_schema, item, ctx)) return false;
        }
        ++idx;
      }
    }

    if (node->contains_schema) {
      uint64_t match_count = 0;
      for (auto item : arr) {
        if (validate_fast(node->contains_schema, item, ctx)) ++match_count;
      }
      uint64_t min_c = node->min_contains.value_or(1);
      uint64_t max_c = node->max_contains.value_or(arr_size);
      if (match_count < min_c || match_count > max_c) return false;
    }
  }

  // Object
  if (vtype == et::OBJECT) {
    dom::object obj; value.get(obj);

    if (node->min_properties.has_value() || node->max_properties.has_value()) {
      uint64_t n = 0;
      for ([[maybe_unused]] auto _ : obj) ++n;
      if (node->min_properties.has_value() && n < node->min_properties.value()) return false;
      if (node->max_properties.has_value() && n > node->max_properties.value()) return false;
    }

    for (const auto& req : node->required) {
      dom::element d;
      if (obj[req].get(d) != SUCCESS) [[unlikely]] return false;
    }

    for (auto [key, val] : obj) {
      std::string_view key_sv(key);
      bool matched = false;

      auto it = node->properties.find(std::string(key_sv));
      if (it != node->properties.end()) {
        if (!validate_fast(it->second, val, ctx)) return false;
        matched = true;
      }

      for (const auto& pp : node->pattern_properties) {
#ifndef ATA_NO_RE2
        if (pp.compiled && re2::RE2::PartialMatch(
            re2::StringPiece(key_sv.data(), key_sv.size()), *pp.compiled)) {
          if (!validate_fast(pp.schema, val, ctx)) return false;
          matched = true;
        }
#endif
      }

      if (!matched) {
        if (node->additional_properties_bool.has_value() &&
            !node->additional_properties_bool.value()) return false;
        if (node->additional_properties_schema &&
            !validate_fast(node->additional_properties_schema, val, ctx)) return false;
      }
    }

    for (const auto& [prop, deps] : node->dependent_required) {
      dom::element d;
      if (obj[prop].get(d) == SUCCESS) {
        for (const auto& dep : deps) {
          dom::element dd;
          if (obj[dep].get(dd) != SUCCESS) return false;
        }
      }
    }

    for (const auto& [prop, schema] : node->dependent_schemas) {
      dom::element d;
      if (obj[prop].get(d) == SUCCESS) {
        if (!validate_fast(schema, value, ctx)) return false;
      }
    }
  }

  // allOf
  for (const auto& sub : node->all_of) {
    if (!validate_fast(sub, value, ctx)) return false;
  }

  // anyOf
  if (!node->any_of.empty()) {
    bool any = false;
    for (const auto& sub : node->any_of) {
      if (validate_fast(sub, value, ctx)) { any = true; break; }
    }
    if (!any) return false;
  }

  // oneOf
  if (!node->one_of.empty()) {
    int n = 0;
    for (const auto& sub : node->one_of) {
      if (validate_fast(sub, value, ctx)) ++n;
      if (n > 1) return false;
    }
    if (n != 1) return false;
  }

  // not
  if (node->not_schema) {
    if (validate_fast(node->not_schema, value, ctx)) return false;
  }

  // if/then/else
  if (node->if_schema) {
    if (validate_fast(node->if_schema, value, ctx)) {
      if (node->then_schema && !validate_fast(node->then_schema, value, ctx)) return false;
    } else {
      if (node->else_schema && !validate_fast(node->else_schema, value, ctx)) return false;
    }
  }

  return true;
}

// --- Codegen compiler ---
static void cg_compile(const schema_node* n, cg::plan& p,
                        std::vector<cg::ins>& out) {
  if (!n) return;
  if (n->boolean_schema.has_value()) {
    if (!*n->boolean_schema) out.push_back({cg::op::EXPECT_NULL});
    return;
  }
  // Composition fallback
  if (!n->ref.empty() || !n->dynamic_ref.empty() || !n->all_of.empty() ||
      !n->any_of.empty() || !n->one_of.empty() || n->not_schema ||
      n->if_schema) {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(n);
    out.push_back({cg::op::COMPOSITION, (uint32_t)(ptr & 0xFFFFFFFF),
                   (uint32_t)((ptr >> 32) & 0xFFFFFFFF)});
    return;
  }
  // Type
  if (n->type_mask) {
    int popcount = __builtin_popcount(n->type_mask);
    if (popcount == 1) {
      // Single type — emit specific opcode
      for (int b = 0; b < 7; ++b) {
        if (n->type_mask & (1u << b)) {
          switch (static_cast<json_type>(b)) {
            case json_type::object:     out.push_back({cg::op::EXPECT_OBJECT}); break;
            case json_type::array:      out.push_back({cg::op::EXPECT_ARRAY}); break;
            case json_type::string:     out.push_back({cg::op::EXPECT_STRING}); break;
            case json_type::number:     out.push_back({cg::op::EXPECT_NUMBER}); break;
            case json_type::integer:    out.push_back({cg::op::EXPECT_INTEGER}); break;
            case json_type::boolean:    out.push_back({cg::op::EXPECT_BOOLEAN}); break;
            case json_type::null_value: out.push_back({cg::op::EXPECT_NULL}); break;
          }
          break;
        }
      }
    } else {
      uint32_t i = (uint32_t)p.type_masks.size();
      p.type_masks.push_back(n->type_mask);
      out.push_back({cg::op::EXPECT_TYPE_MULTI, i});
    }
  }
  // Enum
  if (!n->enum_values_minified.empty()) {
    bool all_str = true;
    for (auto& e : n->enum_values_minified)
      if (e.empty() || e[0]!='"') { all_str=false; break; }
    uint32_t i = (uint32_t)p.enum_sets.size();
    p.enum_sets.push_back(n->enum_values_minified);
    out.push_back({all_str ? cg::op::CHECK_ENUM_STR : cg::op::CHECK_ENUM, i});
  }
  if (n->const_value_raw.has_value()) {
    uint32_t i=(uint32_t)p.strings.size();
    p.strings.push_back(*n->const_value_raw);
    out.push_back({cg::op::CHECK_CONST, i});
  }
  // Numeric
  if (n->minimum.has_value()) { uint32_t i=(uint32_t)p.doubles.size(); p.doubles.push_back(*n->minimum); out.push_back({cg::op::CHECK_MINIMUM,i}); }
  if (n->maximum.has_value()) { uint32_t i=(uint32_t)p.doubles.size(); p.doubles.push_back(*n->maximum); out.push_back({cg::op::CHECK_MAXIMUM,i}); }
  if (n->exclusive_minimum.has_value()) { uint32_t i=(uint32_t)p.doubles.size(); p.doubles.push_back(*n->exclusive_minimum); out.push_back({cg::op::CHECK_EX_MINIMUM,i}); }
  if (n->exclusive_maximum.has_value()) { uint32_t i=(uint32_t)p.doubles.size(); p.doubles.push_back(*n->exclusive_maximum); out.push_back({cg::op::CHECK_EX_MAXIMUM,i}); }
  if (n->multiple_of.has_value()) { uint32_t i=(uint32_t)p.doubles.size(); p.doubles.push_back(*n->multiple_of); out.push_back({cg::op::CHECK_MULTIPLE_OF,i}); }
  // String
  if (n->min_length.has_value()) out.push_back({cg::op::CHECK_MIN_LENGTH,(uint32_t)*n->min_length});
  if (n->max_length.has_value()) out.push_back({cg::op::CHECK_MAX_LENGTH,(uint32_t)*n->max_length});
#ifndef ATA_NO_RE2
  if (n->compiled_pattern) { uint32_t i=(uint32_t)p.regexes.size(); p.regexes.push_back(n->compiled_pattern); out.push_back({cg::op::CHECK_PATTERN,i}); }
#endif
  if (n->format.has_value()) {
    uint32_t i=(uint32_t)p.format_ids.size();
    p.format_ids.push_back(n->format_id);
    out.push_back({cg::op::CHECK_FORMAT,i});
  }
  // Array
  if (n->min_items.has_value()) out.push_back({cg::op::CHECK_MIN_ITEMS,(uint32_t)*n->min_items});
  if (n->max_items.has_value()) out.push_back({cg::op::CHECK_MAX_ITEMS,(uint32_t)*n->max_items});
  if (n->unique_items) out.push_back({cg::op::CHECK_UNIQUE_ITEMS});
  if (n->items_schema) {
    uint32_t si=(uint32_t)p.subs.size();
    p.subs.emplace_back();
    std::vector<cg::ins> sub_code;
    cg_compile(n->items_schema.get(), p, sub_code);
    sub_code.push_back({cg::op::END});
    p.subs[si] = std::move(sub_code);
    out.push_back({cg::op::ARRAY_ITEMS, si});
  }
  // Object
  for (auto& r : n->required) { uint32_t i=(uint32_t)p.strings.size(); p.strings.push_back(r); out.push_back({cg::op::CHECK_REQUIRED,i}); }
  if (n->min_properties.has_value()) out.push_back({cg::op::CHECK_MIN_PROPS,(uint32_t)*n->min_properties});
  if (n->max_properties.has_value()) out.push_back({cg::op::CHECK_MAX_PROPS,(uint32_t)*n->max_properties});
  // additional_properties_schema requires tree walker — bail out to COMPOSITION
  if (n->additional_properties_schema) {
    out.push_back({cg::op::COMPOSITION, 0, 0});
    return;
  }
  if (!n->properties.empty() || (n->additional_properties_bool.has_value() && !*n->additional_properties_bool)) {
    out.push_back({cg::op::OBJ_PROPS_START});
    if (n->additional_properties_bool.has_value() && !*n->additional_properties_bool)
      out.push_back({cg::op::CHECK_NO_ADDITIONAL});
    for (auto& [name, schema] : n->properties) {
      uint32_t ni=(uint32_t)p.strings.size(); p.strings.push_back(name);
      uint32_t si=(uint32_t)p.subs.size();
      p.subs.emplace_back();
      std::vector<cg::ins> sub_code;
      cg_compile(schema.get(), p, sub_code);
      sub_code.push_back({cg::op::END});
      p.subs[si] = std::move(sub_code);
      out.push_back({cg::op::OBJ_PROP, ni, si});
    }
    out.push_back({cg::op::OBJ_PROPS_END});
  }
}

// --- Codegen executor ---

static bool cg_exec(const cg::plan& p, const std::vector<cg::ins>& code,
                     dom::element value) {
  auto t = value.type();
  bool t_numeric = (t == et::INT64 || t == et::UINT64 || t == et::DOUBLE);
  double t_dval = t_numeric ? to_double(value) : 0.0;
  for (size_t i=0; i<code.size(); ++i) {
    auto& c = code[i];
    switch(c.o) {
    case cg::op::END: return true;
    case cg::op::EXPECT_OBJECT: if(t!=et::OBJECT) return false; break;
    case cg::op::EXPECT_ARRAY: if(t!=et::ARRAY) return false; break;
    case cg::op::EXPECT_STRING: if(t!=et::STRING) return false; break;
    case cg::op::EXPECT_NUMBER: if(!t_numeric) return false; break;
    case cg::op::EXPECT_INTEGER: if(t!=et::INT64&&t!=et::UINT64) return false; break;
    case cg::op::EXPECT_BOOLEAN: if(t!=et::BOOL) return false; break;
    case cg::op::EXPECT_NULL: if(t!=et::NULL_VALUE) return false; break;
    case cg::op::EXPECT_TYPE_MULTI: {
      if(!(element_type_mask(t) & p.type_masks[c.a])) return false; break;
    }
    case cg::op::CHECK_MINIMUM: if(t_numeric&&t_dval<p.doubles[c.a])return false; break;
    case cg::op::CHECK_MAXIMUM: if(t_numeric&&t_dval>p.doubles[c.a])return false; break;
    case cg::op::CHECK_EX_MINIMUM: if(t_numeric&&t_dval<=p.doubles[c.a])return false; break;
    case cg::op::CHECK_EX_MAXIMUM: if(t_numeric&&t_dval>=p.doubles[c.a])return false; break;
    case cg::op::CHECK_MULTIPLE_OF: if(t_numeric){double d=p.doubles[c.a],r=std::fmod(t_dval,d);if(std::abs(r)>1e-8&&std::abs(r-d)>1e-8)return false;} break;
    case cg::op::CHECK_MIN_LENGTH: if(t==et::STRING){std::string_view sv;value.get(sv);if(utf8_length(sv)<c.a)return false;} break;
    case cg::op::CHECK_MAX_LENGTH: if(t==et::STRING){std::string_view sv;value.get(sv);if(utf8_length(sv)>c.a)return false;} break;
#ifndef ATA_NO_RE2
    case cg::op::CHECK_PATTERN: if(t==et::STRING){std::string_view sv;value.get(sv);if(!re2::RE2::PartialMatch(re2::StringPiece(sv.data(),sv.size()),*p.regexes[c.a]))return false;} break;
#else
    case cg::op::CHECK_PATTERN: break;
#endif
    case cg::op::CHECK_FORMAT: if(t==et::STRING){std::string_view sv;value.get(sv);if(!check_format_by_id(sv,p.format_ids[c.a]))return false;} break;
    case cg::op::CHECK_MIN_ITEMS: if(t==et::ARRAY){dom::array a;value.get(a);uint64_t s=0;for([[maybe_unused]]auto _:a)++s;if(s<c.a)return false;} break;
    case cg::op::CHECK_MAX_ITEMS: if(t==et::ARRAY){dom::array a;value.get(a);uint64_t s=0;for([[maybe_unused]]auto _:a)++s;if(s>c.a)return false;} break;
    case cg::op::CHECK_UNIQUE_ITEMS: if(t==et::ARRAY){dom::array a;value.get(a);std::set<std::string> seen;for(auto x:a)if(!seen.insert(canonical_json(x)).second)return false;} break;
    case cg::op::ARRAY_ITEMS: if(t==et::ARRAY){dom::array a;value.get(a);for(auto x:a)if(!cg_exec(p,p.subs[c.a],x))return false;} break;
    case cg::op::CHECK_REQUIRED: if(t==et::OBJECT){dom::object o;value.get(o);dom::element d;if(o[p.strings[c.a]].get(d)!=SUCCESS)return false;} break;
    case cg::op::CHECK_MIN_PROPS: if(t==et::OBJECT){dom::object o;value.get(o);uint64_t n=0;for([[maybe_unused]]auto _:o)++n;if(n<c.a)return false;} break;
    case cg::op::CHECK_MAX_PROPS: if(t==et::OBJECT){dom::object o;value.get(o);uint64_t n=0;for([[maybe_unused]]auto _:o)++n;if(n>c.a)return false;} break;
    case cg::op::OBJ_PROPS_START: if(t==et::OBJECT){
      dom::object o; value.get(o);
      // collect prop defs
      struct pd{std::string_view nm;uint32_t si;};
      std::vector<pd> props; bool no_add=false;
      size_t j=i+1;
      for(;j<code.size()&&code[j].o!=cg::op::OBJ_PROPS_END;++j){
        if(code[j].o==cg::op::OBJ_PROP) props.push_back({p.strings[code[j].a],code[j].b});
        else if(code[j].o==cg::op::CHECK_NO_ADDITIONAL) no_add=true;
      }
      for(auto [key,val]:o){
        bool matched=false;
        for(auto& pp:props){if(key==pp.nm){if(!cg_exec(p,p.subs[pp.si],val))return false;matched=true;break;}}
        if(!matched&&no_add)return false;
      }
      i=j; break;
    } else { /* skip to OBJ_PROPS_END */ size_t j=i+1; for(;j<code.size()&&code[j].o!=cg::op::OBJ_PROPS_END;++j); i=j; } break;
    case cg::op::OBJ_PROP: case cg::op::OBJ_PROPS_END: case cg::op::CHECK_NO_ADDITIONAL: break;
    case cg::op::CHECK_ENUM_STR: {
      auto& es=p.enum_sets[c.a]; bool f=false;
      if(t==et::STRING){std::string_view sv;value.get(sv);for(auto& e:es)if(e.size()==sv.size()+2&&e[0]=='"'&&e.back()=='"'&&e.compare(1,sv.size(),sv)==0){f=true;break;}}
      if(!f){std::string v=canonical_json(value);for(auto& e:es)if(e==v){f=true;break;}}
      if(!f)return false; break;
    }
    case cg::op::CHECK_ENUM: {
      auto& es=p.enum_sets[c.a]; bool f=false;
      if(t==et::STRING){std::string_view sv;value.get(sv);for(auto& e:es)if(e.size()==sv.size()+2&&e[0]=='"'&&e.back()=='"'&&e.compare(1,sv.size(),sv)==0){f=true;break;}}
      if(!f&&value.is<int64_t>()){int64_t v;value.get(v);auto s=std::to_string(v);for(auto& e:es)if(e==s){f=true;break;}}
      if(!f){std::string v=canonical_json(value);for(auto& e:es)if(e==v){f=true;break;}}
      if(!f)return false; break;
    }
    case cg::op::CHECK_CONST: if(canonical_json(value)!=p.strings[c.a])return false; break;
    case cg::op::COMPOSITION: return false; // fallback to tree walker
    }
  }
  return true;
}

// --- On Demand fast path executor ---
// Uses simdjson On Demand API to avoid materializing the full DOM tree.
// Returns: true = valid, false = invalid OR unsupported (fallback to DOM).

static json_type od_type(simdjson::ondemand::value& v) {
  simdjson::ondemand::json_type jt;
  if (v.type().get(jt)) return json_type::null_value;
  switch (jt) {
    case simdjson::ondemand::json_type::object: return json_type::object;
    case simdjson::ondemand::json_type::array: return json_type::array;
    case simdjson::ondemand::json_type::string: return json_type::string;
    case simdjson::ondemand::json_type::boolean: return json_type::boolean;
    case simdjson::ondemand::json_type::null: return json_type::null_value;
    case simdjson::ondemand::json_type::number: {
      simdjson::ondemand::number_type nt;
      if (v.get_number_type().get(nt) == SUCCESS &&
          nt == simdjson::ondemand::number_type::floating_point_number)
        return json_type::number;
      return json_type::integer;
    }
  }
  return json_type::string;
}

static bool od_exec(const cg::plan& p, const std::vector<cg::ins>& code,
                     simdjson::ondemand::value value) {
  auto t = od_type(value);
  bool t_numeric = (t == json_type::integer || t == json_type::number);
  for (size_t i = 0; i < code.size(); ++i) {
    auto& c = code[i];
    switch (c.o) {
    case cg::op::END: return true;
    case cg::op::EXPECT_OBJECT: if(t!=json_type::object) return false; break;
    case cg::op::EXPECT_ARRAY: if(t!=json_type::array) return false; break;
    case cg::op::EXPECT_STRING: if(t!=json_type::string) return false; break;
    case cg::op::EXPECT_NUMBER: if(!t_numeric) return false; break;
    case cg::op::EXPECT_INTEGER: if(t!=json_type::integer) return false; break;
    case cg::op::EXPECT_BOOLEAN: if(t!=json_type::boolean) return false; break;
    case cg::op::EXPECT_NULL: if(t!=json_type::null_value) return false; break;
    case cg::op::EXPECT_TYPE_MULTI: {
      // integer matches both "integer" and "number" type constraints
      uint8_t tbits = json_type_bit(t);
      if (t == json_type::integer) tbits |= json_type_bit(json_type::number);
      if(!(tbits & p.type_masks[c.a])) return false; break;
    }
    case cg::op::CHECK_MINIMUM:
    case cg::op::CHECK_MAXIMUM:
    case cg::op::CHECK_EX_MINIMUM:
    case cg::op::CHECK_EX_MAXIMUM:
    case cg::op::CHECK_MULTIPLE_OF: {
      if (t_numeric) {
        double v;
        if (t==json_type::integer) { int64_t iv; if(value.get(iv)!=SUCCESS) return false; v=(double)iv; }
        else { if(value.get(v)!=SUCCESS) return false; }
        double d=p.doubles[c.a];
        if(c.o==cg::op::CHECK_MINIMUM && v<d) return false;
        if(c.o==cg::op::CHECK_MAXIMUM && v>d) return false;
        if(c.o==cg::op::CHECK_EX_MINIMUM && v<=d) return false;
        if(c.o==cg::op::CHECK_EX_MAXIMUM && v>=d) return false;
        if(c.o==cg::op::CHECK_MULTIPLE_OF){double r=std::fmod(v,d);if(std::abs(r)>1e-8&&std::abs(r-d)>1e-8)return false;}
      }
      break;
    }
    case cg::op::CHECK_MIN_LENGTH: if(t==json_type::string){std::string_view sv; if(value.get(sv)!=SUCCESS) return false; if(utf8_length(sv)<c.a) return false;} break;
    case cg::op::CHECK_MAX_LENGTH: if(t==json_type::string){std::string_view sv; if(value.get(sv)!=SUCCESS) return false; if(utf8_length(sv)>c.a) return false;} break;
#ifndef ATA_NO_RE2
    case cg::op::CHECK_PATTERN: if(t==json_type::string){std::string_view sv; if(value.get(sv)!=SUCCESS) return false; if(!re2::RE2::PartialMatch(re2::StringPiece(sv.data(),sv.size()),*p.regexes[c.a]))return false;} break;
#else
    case cg::op::CHECK_PATTERN: break;
#endif
    case cg::op::CHECK_FORMAT: if(t==json_type::string){std::string_view sv; if(value.get(sv)!=SUCCESS) return false; if(!check_format_by_id(sv,p.format_ids[c.a]))return false;} break;
    case cg::op::CHECK_MIN_ITEMS: if(t==json_type::array){
      simdjson::ondemand::array a; if(value.get(a)!=SUCCESS) return false;
      uint64_t s=0; for(auto x:a){(void)x;++s;} if(s<c.a) return false;
    } break;
    case cg::op::CHECK_MAX_ITEMS: if(t==json_type::array){
      simdjson::ondemand::array a; if(value.get(a)!=SUCCESS) return false;
      uint64_t s=0; for(auto x:a){(void)x;++s;} if(s>c.a) return false;
    } break;
    case cg::op::ARRAY_ITEMS: if(t==json_type::array){
      simdjson::ondemand::array a; if(value.get(a)!=SUCCESS) return false;
      for(auto elem:a){
        simdjson::ondemand::value v; if(elem.get(v)!=SUCCESS) return false;
        if(!od_exec(p,p.subs[c.a],v)) return false;
      }
    } break;
    case cg::op::CHECK_REQUIRED: if(t==json_type::object){
      simdjson::ondemand::object o; if(value.get(o)!=SUCCESS) return false;
      auto f = o.find_field_unordered(p.strings[c.a]);
      if(f.error()) return false;
    } break;
    case cg::op::CHECK_MIN_PROPS: if(t==json_type::object){
      simdjson::ondemand::object o; if(value.get(o)!=SUCCESS) return false;
      uint64_t n=0; for(auto f:o){(void)f;++n;} if(n<c.a) return false;
    } break;
    case cg::op::CHECK_MAX_PROPS: if(t==json_type::object){
      simdjson::ondemand::object o; if(value.get(o)!=SUCCESS) return false;
      uint64_t n=0; for(auto f:o){(void)f;++n;} if(n>c.a) return false;
    } break;
    case cg::op::OBJ_PROPS_START: if(t==json_type::object){
      simdjson::ondemand::object o; if(value.get(o)!=SUCCESS) return false;
      struct pd{std::string_view nm;uint32_t si;};
      std::vector<pd> props; bool no_add=false;
      size_t j=i+1;
      for(;j<code.size()&&code[j].o!=cg::op::OBJ_PROPS_END;++j){
        if(code[j].o==cg::op::OBJ_PROP) props.push_back({p.strings[code[j].a],code[j].b});
        else if(code[j].o==cg::op::CHECK_NO_ADDITIONAL) no_add=true;
      }
      for(auto field:o){
        simdjson::ondemand::raw_json_string rk; if(field.key().get(rk)!=SUCCESS) return false;
        std::string_view key;
        if (field.unescaped_key().get(key)) continue;
        bool matched=false;
        for(auto& pp:props){
          if(key==pp.nm){
            simdjson::ondemand::value fv; if(field.value().get(fv)!=SUCCESS) return false;
            if(!od_exec(p,p.subs[pp.si],fv)) return false;
            matched=true; break;
          }
        }
        if(!matched&&no_add) return false;
      }
      i=j; break;
    } else { size_t j=i+1; for(;j<code.size()&&code[j].o!=cg::op::OBJ_PROPS_END;++j); i=j; } break;
    case cg::op::OBJ_PROP: case cg::op::OBJ_PROPS_END: case cg::op::CHECK_NO_ADDITIONAL: break;

    // These require full materialization — bail to DOM path
    case cg::op::CHECK_UNIQUE_ITEMS:
    case cg::op::CHECK_ENUM_STR:
    case cg::op::CHECK_ENUM:
    case cg::op::CHECK_CONST:
    case cg::op::COMPOSITION:
      return false;
    }
  }
  return true;
}

// Determine if a codegen plan can use On Demand (no enum/const/uniqueItems)
static bool plan_supports_ondemand(const cg::plan& p) {
  for (auto& c : p.code) {
    if (c.o == cg::op::CHECK_UNIQUE_ITEMS || c.o == cg::op::CHECK_ENUM_STR ||
        c.o == cg::op::CHECK_ENUM || c.o == cg::op::CHECK_CONST ||
        c.o == cg::op::COMPOSITION)
      return false;
  }
  // Also check sub-plans
  for (auto& sub : p.subs) {
    for (auto& c : sub) {
      if (c.o == cg::op::CHECK_UNIQUE_ITEMS || c.o == cg::op::CHECK_ENUM_STR ||
          c.o == cg::op::CHECK_ENUM || c.o == cg::op::CHECK_CONST ||
          c.o == cg::op::COMPOSITION)
        return false;
    }
  }
  return true;
}

// Free padding: check if buffer is near a page boundary
// On modern systems, pages are at least 4096 bytes. If we're far enough
// from the end of a page, we can read 64 bytes beyond without a fault.
static long get_page_size() {
#ifdef _WIN32
  SYSTEM_INFO si; GetSystemInfo(&si); return si.dwPageSize;
#else
  static long ps = sysconf(_SC_PAGESIZE);
  return ps;
#endif
}

static bool near_page_boundary(const char* buf, size_t len) {
  return ((reinterpret_cast<uintptr_t>(buf + len - 1) % get_page_size())
          + REQUIRED_PADDING >= static_cast<uintptr_t>(get_page_size()));
}

// Zero-copy validate with free padding (Lemire's trick).
// Almost never allocates — only if buffer is near a page boundary.
static simdjson::padded_string_view get_free_padded_view(
    const char* data, size_t length, simdjson::padded_string& fallback) {
  if (near_page_boundary(data, length)) {
    // Rare: near page boundary, must copy
    fallback = simdjson::padded_string(data, length);
    return fallback;
  }
  // Common: free padding available, zero-copy
  return simdjson::padded_string_view(data, length, length + REQUIRED_PADDING);
}

// Build an od_plan from a schema_node tree.
static od_plan_ptr compile_od_plan(const schema_node_ptr& node) {
  if (!node) return nullptr;

  auto plan = std::make_shared<od_plan>();

  if (node->boolean_schema.has_value()) {
    if (!node->boolean_schema.value()) plan->supported = false;
    return plan;
  }

  // Unsupported features → fall back to DOM
  if (!node->ref.empty() ||
      !node->enum_values_minified.empty() ||
      node->const_value_raw.has_value() ||
      node->unique_items ||
      !node->all_of.empty() ||
      !node->any_of.empty() ||
      !node->one_of.empty() ||
      node->not_schema ||
      node->if_schema ||
      node->contains_schema ||
      !node->prefix_items.empty() ||
      !node->pattern_properties.empty() ||
      !node->dependent_required.empty() ||
      !node->dependent_schemas.empty() ||
      node->property_names_schema ||
      node->additional_properties_schema) {
    plan->supported = false;
    return plan;
  }

  plan->type_mask = node->type_mask;
  if (node->minimum) { plan->num_flags |= od_plan::HAS_MIN; plan->num_min = *node->minimum; }
  if (node->maximum) { plan->num_flags |= od_plan::HAS_MAX; plan->num_max = *node->maximum; }
  if (node->exclusive_minimum) { plan->num_flags |= od_plan::HAS_EX_MIN; plan->num_ex_min = *node->exclusive_minimum; }
  if (node->exclusive_maximum) { plan->num_flags |= od_plan::HAS_EX_MAX; plan->num_ex_max = *node->exclusive_maximum; }
  if (node->multiple_of) { plan->num_flags |= od_plan::HAS_MUL; plan->num_mul = *node->multiple_of; }
  plan->min_length = node->min_length;
  plan->max_length = node->max_length;
#ifndef ATA_NO_RE2
  plan->pattern = node->compiled_pattern.get();
#endif
  plan->format_id = node->format_id;

  // Object plan — build hash lookup for O(1) per-field dispatch
  if (!node->properties.empty() || !node->required.empty() ||
      node->additional_properties_bool.has_value() ||
      node->min_properties.has_value() || node->max_properties.has_value()) {
    auto op = std::make_shared<od_plan::obj_plan>();
    op->required_count = node->required.size();
    op->min_props = node->min_properties;
    op->max_props = node->max_properties;
    if (node->additional_properties_bool.has_value() &&
        !node->additional_properties_bool.value()) {
      op->no_additional = true;
    }
    // Build merged entries: each key appears once with required_idx + sub_plan
    std::unordered_map<std::string, size_t> key_to_idx;
    // Register required keys
    for (size_t i = 0; i < node->required.size() && i < 64; i++) {
      auto& rk = node->required[i];
      if (key_to_idx.find(rk) == key_to_idx.end()) {
        key_to_idx[rk] = op->entries.size();
        op->entries.push_back({rk, static_cast<int>(i), nullptr});
      } else {
        op->entries[key_to_idx[rk]].required_idx = static_cast<int>(i);
      }
    }
    // Register properties + compile sub-plans
    for (auto& [key, sub_node] : node->properties) {
      auto sub = compile_od_plan(sub_node);
      if (!sub || !sub->supported) { plan->supported = false; return plan; }
      auto it = key_to_idx.find(key);
      if (it != key_to_idx.end()) {
        op->entries[it->second].sub = std::move(sub);
      } else {
        key_to_idx[key] = op->entries.size();
        op->entries.push_back({key, -1, std::move(sub)});
      }
    }
    plan->object = std::move(op);
  }

  // Array plan
  if (node->items_schema || node->min_items.has_value() || node->max_items.has_value()) {
    auto ap = std::make_shared<od_plan::arr_plan>();
    ap->min_items = node->min_items;
    ap->max_items = node->max_items;
    if (node->items_schema) {
      ap->items = compile_od_plan(node->items_schema);
      if (!ap->items || !ap->items->supported) { plan->supported = false; return plan; }
    }
    plan->array = std::move(ap);
  }

  return plan;
}

// Fast ASCII check: if all bytes < 0x80, byte length == codepoint length
static inline uint64_t utf8_length_fast(std::string_view s) {
  // Check 8 bytes at a time for non-ASCII
  const uint8_t* p = reinterpret_cast<const uint8_t*>(s.data());
  size_t n = s.size();
  size_t i = 0;
  uint64_t has_high = 0;
  for (; i + 8 <= n; i += 8) {
    uint64_t block;
    std::memcpy(&block, p + i, 8);
    has_high |= block & 0x8080808080808080ULL;
  }
  for (; i < n; i++) has_high |= p[i] & 0x80;
  if (has_high == 0) return n;  // Pure ASCII — byte count == codepoint count
  return utf8_length(s);        // Fallback to full counting
}

// Execute an od_plan against a simdjson On-Demand value.
// Each value consumed exactly once. Uses simdjson types directly — no od_type() overhead.
static bool od_exec_plan(const od_plan& plan, simdjson::ondemand::value value) {
  // Use simdjson type directly — skip od_type() conversion + get_number_type()
  using sjt = simdjson::ondemand::json_type;
  sjt st;
  if (value.type().get(st) != SUCCESS) return false;

  // Type check using simdjson type directly
  if (plan.type_mask) {
    uint8_t tbits;
    switch (st) {
      case sjt::string:  tbits = json_type_bit(json_type::string); break;
      case sjt::boolean: tbits = json_type_bit(json_type::boolean); break;
      case sjt::null:    tbits = json_type_bit(json_type::null_value); break;
      case sjt::object:  tbits = json_type_bit(json_type::object); break;
      case sjt::array:   tbits = json_type_bit(json_type::array); break;
      case sjt::number:
        // Only call get_number_type when schema has type constraint that distinguishes int/number
        tbits = json_type_bit(json_type::number) | json_type_bit(json_type::integer);
        if ((plan.type_mask & tbits) != tbits) {
          // Schema distinguishes — need to check actual number type
          simdjson::ondemand::number_type nt;
          if (value.get_number_type().get(nt) == SUCCESS &&
              nt != simdjson::ondemand::number_type::floating_point_number)
            tbits = json_type_bit(json_type::integer) | json_type_bit(json_type::number);
          else
            tbits = json_type_bit(json_type::number);
        }
        break;
      default: tbits = 0;
    }
    if (!(tbits & plan.type_mask)) return false;
  }

  switch (st) {
  case sjt::number: {
    if (!plan.num_flags) break;  // No numeric constraints
    double v;
    // Try integer first (more common), fall back to double
    int64_t iv;
    if (value.get(iv) == SUCCESS) {
      v = static_cast<double>(iv);
    } else if (value.get(v) != SUCCESS) {
      return false;
    }
    uint8_t f = plan.num_flags;
    if ((f & od_plan::HAS_MIN) && v < plan.num_min) return false;
    if ((f & od_plan::HAS_MAX) && v > plan.num_max) return false;
    if ((f & od_plan::HAS_EX_MIN) && v <= plan.num_ex_min) return false;
    if ((f & od_plan::HAS_EX_MAX) && v >= plan.num_ex_max) return false;
    if (f & od_plan::HAS_MUL) {
      double r = std::fmod(v, plan.num_mul);
      if (std::abs(r) > 1e-8 && std::abs(r - plan.num_mul) > 1e-8) return false;
    }
    break;
  }
  case sjt::string: {
    std::string_view sv;
    if (value.get(sv) != SUCCESS) return false;
    if (plan.min_length || plan.max_length) {
      uint64_t len = utf8_length_fast(sv);
      if (plan.min_length && len < *plan.min_length) return false;
      if (plan.max_length && len > *plan.max_length) return false;
    }
#ifndef ATA_NO_RE2
    if (plan.pattern) {
      if (!re2::RE2::PartialMatch(re2::StringPiece(sv.data(), sv.size()), *plan.pattern))
        return false;
    }
#endif
    if (plan.format_id != 255) {
      if (!check_format_by_id(sv, plan.format_id)) return false;
    }
    break;
  }
  case sjt::object: {
    if (!plan.object) break;
    auto& op = *plan.object;
    simdjson::ondemand::object obj;
    if (value.get(obj) != SUCCESS) return false;

    uint64_t required_found = 0;
    uint64_t prop_count = 0;

    for (auto field : obj) {
      std::string_view key;
      if (field.unescaped_key().get(key)) continue;
      prop_count++;

      // Single merged scan: required + property in one pass
      bool matched = false;
      for (auto& e : op.entries) {
        if (key == e.key) {
          if (e.required_idx >= 0)
            required_found |= (1ULL << e.required_idx);
          if (e.sub) {
            simdjson::ondemand::value fv;
            if (field.value().get(fv) != SUCCESS) return false;
            if (!od_exec_plan(*e.sub, fv)) return false;
          }
          matched = true;
          break;
        }
      }
      if (!matched && op.no_additional) return false;
    }

    uint64_t required_mask = (op.required_count >= 64)
        ? ~0ULL : ((1ULL << op.required_count) - 1);
    if ((required_found & required_mask) != required_mask) return false;
    if (op.min_props && prop_count < *op.min_props) return false;
    if (op.max_props && prop_count > *op.max_props) return false;
    break;
  }
  case sjt::array: {
    if (!plan.array) break;
    auto& ap = *plan.array;
    simdjson::ondemand::array arr;
    if (value.get(arr) != SUCCESS) return false;

    uint64_t count = 0;
    for (auto elem : arr) {
      simdjson::ondemand::value v;
      if (elem.get(v) != SUCCESS) return false;
      if (ap.items && !od_exec_plan(*ap.items, v)) return false;
      count++;
    }
    if (ap.min_items && count < *ap.min_items) return false;
    if (ap.max_items && count > *ap.max_items) return false;
    break;
  }
  default:
    break;
  }

  return true;
}

schema_ref compile(std::string_view schema_json) {
  auto ctx = std::make_shared<compiled_schema>();
  ctx->raw_schema = std::string(schema_json);

  dom::element doc;
  auto result = ctx->parser.parse(ctx->raw_schema);
  if (result.error()) {
    return schema_ref{nullptr};
  }
  doc = result.value_unsafe();

  ctx->root = compile_node(doc, *ctx);

  if (!ctx->compile_error.empty()) {
    return schema_ref{nullptr};
  }

  // Generate codegen plan
  cg_compile(ctx->root.get(), ctx->gen_plan, ctx->gen_plan.code);
  ctx->gen_plan.code.push_back({cg::op::END});
  ctx->use_ondemand = plan_supports_ondemand(ctx->gen_plan);
  ctx->od = compile_od_plan(ctx->root);

  schema_ref ref;
  ref.impl = ctx;
  ref.warnings = std::move(ctx->warnings);
  return ref;
}

validation_result validate(const schema_ref& schema, std::string_view json,
                           const validate_options& opts) {
  if (!schema.impl || !schema.impl->root) {
    return {false, {{error_code::invalid_schema, "", "schema not compiled"}}};
  }

  // Free padding trick: avoid padded_string copy when possible
  simdjson::padded_string fallback;
  auto psv = get_free_padded_view(json.data(), json.size(), fallback);

  // Ultra-fast path: On Demand (no DOM materialization)
  static constexpr size_t OD_THRESHOLD = 32;
  if (schema.impl->use_ondemand && !schema.impl->gen_plan.code.empty() &&
      json.size() >= OD_THRESHOLD) {
    auto od_result = tl_od_parser().iterate(psv);
    if (!od_result.error()) {
      simdjson::ondemand::value root_val;
      if (od_result.get_value().get(root_val) == SUCCESS) {
        if (od_exec(schema.impl->gen_plan, schema.impl->gen_plan.code, root_val)) {
          return {true, {}};
        }
      }
    }
    // Need fresh view for DOM parse (On Demand consumed it)
    psv = get_free_padded_view(json.data(), json.size(), fallback);
  }

  auto& dom_p = tl_dom_parser();
  auto result = dom_p.parse(psv);
  if (result.error()) {
    return {false, {{error_code::invalid_json, "", "invalid JSON document"}}};
  }

  // Fast path: codegen bytecode execution (DOM)
  if (!schema.impl->use_ondemand && !schema.impl->gen_plan.code.empty()) {
    if (cg_exec(schema.impl->gen_plan, schema.impl->gen_plan.code,
                result.value_unsafe())) {
      return {true, {}};
    }
    // Codegen said invalid OR hit COMPOSITION — fall through to tree walker
  }

  // Slow path: tree walker with error details (reuse already-parsed DOM)
  std::vector<validation_error> errors;
  if (schema.impl->has_dynamic_refs) {
    dynamic_scope_t scope;
    auto rit = schema.impl->resource_dynamic_anchors.find("");
    if (rit != schema.impl->resource_dynamic_anchors.end()) {
      scope.push_back(&rit->second);
    }
    if (!schema.impl->root->id.empty()) {
      auto iit = schema.impl->resource_dynamic_anchors.find(schema.impl->root->id);
      if (iit != schema.impl->resource_dynamic_anchors.end()) {
        scope.push_back(&iit->second);
      }
    }
    validate_node(schema.impl->root, result.value_unsafe(), "", *schema.impl, errors,
                  opts.all_errors, &scope);
  } else {
    validate_node(schema.impl->root, result.value_unsafe(), "", *schema.impl, errors,
                  opts.all_errors);
  }

  return {errors.empty(), std::move(errors)};
}

validation_result validate(std::string_view schema_json,
                           std::string_view json,
                           const validate_options& opts) {
  auto s = compile(schema_json);
  if (!s) {
    return {false, {{error_code::invalid_schema, "", "failed to compile schema"}}};
  }
  return validate(s, json, opts);
}


bool is_valid_prepadded(const schema_ref& schema, const char* data, size_t length) {
  if (!schema.impl || !schema.impl->root) return false;

  simdjson::padded_string fallback;
  auto psv = get_free_padded_view(data, length, fallback);

  // On-Demand fast path: skip DOM parse entirely
  // Minimum 32 bytes — On-Demand doesn't fully validate small malformed docs
  if (schema.impl->od && schema.impl->od->supported && length >= 32) {
    auto od_result = tl_od_parser().iterate(psv);
    if (!od_result.error()) {
      simdjson::ondemand::value root_val;
      if (od_result.get_value().get(root_val) == SUCCESS) {
        if (od_exec_plan(*schema.impl->od, root_val)) {
          return true;
        }
      }
    }
    psv = get_free_padded_view(data, length, fallback);
  }

  auto result = tl_dom_parser().parse(psv);
  if (result.error()) return false;

  if (!schema.impl->gen_plan.code.empty()) {
    return cg_exec(schema.impl->gen_plan, schema.impl->gen_plan.code, result.value_unsafe());
  }

  return validate_fast(schema.impl->root, result.value_unsafe(), *schema.impl);
}

bool is_valid_buf(const schema_ref& schema, const uint8_t* data, size_t length) {
  if (!schema.impl || !schema.impl->root || !data || length == 0) return false;

  // Thread-local buffer with simdjson padding — reused across calls
  thread_local std::string tl_buf;
  const size_t needed = length + REQUIRED_PADDING;
  if (tl_buf.size() < needed) tl_buf.resize(needed);
  std::memcpy(tl_buf.data(), data, length);
  std::memset(tl_buf.data() + length, 0, REQUIRED_PADDING);

  return is_valid_prepadded(schema, tl_buf.data(), length);
}

}  // namespace ata
/* end file src/ata.cpp */
