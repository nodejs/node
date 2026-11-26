// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/prelude.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "experimental/redline/base/log.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/mutators/explore.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

REDLINE_TUNABLE(kPreludeOnDemand, "prelude.on_demand", 0, 0, 2,
                "give a program only the statements of the default prelude "
                "that it can use (0: off, 1: on, 2: on without the fallback "
                "to the whole prelude)");

namespace {

constexpr char kLog[] = "prelude";

// Keep this small: it runs before every execution. The functions evaluate
// their arguments (so that the values flow) but never throw, except
// assertThrows-style helpers which must run their callback.
const char kDefaultPrelude[] = R"JS(
var __redline_noop = function() {};
var __redline_call = function(f) {
  try { return typeof f === "function" ? f() : eval(String(f)); } catch (e) { return e; }
};
var assertSame = function(a, b) { return a === b; };
var assertNotSame = function(a, b) { return a !== b; };
var assertEquals = function(a, b) { return a == b; };
var assertNotEquals = function(a, b) { return a != b; };
var assertEqualsDelta = function(a, b, d) { return Math.abs(a - b) <= d; };
var assertArrayEquals = function(a, b) { return a && b && a.length === b.length; };
var assertPropertiesEqual = function(a, b) { return a && b && Object.keys(a).length === Object.keys(b).length; };
var assertToStringEquals = function(a, b) { return a == String(b); };
var assertTrue = function(a) { return !!a; };
var assertFalse = function(a) { return !a; };
var assertNull = function(a) { return a === null; };
var assertNotNull = function(a) { return a !== null; };
var assertThrows = function(f) { return __redline_call(f); };
var assertThrowsEquals = function(f) { return __redline_call(f); };
var assertThrowsAtRuntime = function(f) { return __redline_call(f); };
var assertThrowsAsync = function(p) { if (p && p.then) p.then(__redline_noop, __redline_noop); return p; };
var assertDoesNotThrow = function(f) { return __redline_call(f); };
var assertEarlyError = function(s) { try { new Function(s); } catch (e) {} };
var assertException = __redline_call;
var assertUnreachable = __redline_noop;
var assertContains = function(a, b) { return b && b.indexOf && b.indexOf(a) >= 0; };
var assertMatches = function(r, s) { return r && r.test && r.test(s); };
var assertInstanceof = function(o, c) { return o instanceof c; };
var assertPromiseResult = function(p, f, r) {
  if (p && p.then) p.then(typeof f === "function" ? f : __redline_noop, typeof r === "function" ? r : __redline_noop);
  return p;
};
var assertOptimized = __redline_noop;
var assertUnoptimized = __redline_noop;
var assertMaglevved = __redline_noop;
var assertNotMaglevved = __redline_noop;
var fail = __redline_noop;
var failWithMessage = __redline_noop;
var isNeverOptimize = function() { return false; };
var isNeverOptimizeLiteMode = function() { return false; };
var isLazy = function() { return false; };
var isInterpreted = function(f) { return %ActiveTierIsIgnition(f); };
var isBaseline = function(f) { return %ActiveTierIsSparkplug(f); };
var isUnoptimized = function(f) { return !%ActiveTierIsMaglev(f) && !%ActiveTierIsTurbofan(f); };
var isOptimized = function(f) { return %ActiveTierIsMaglev(f) || %ActiveTierIsTurbofan(f); };
var isMaglevved = function(f) { return %ActiveTierIsMaglev(f); };
var isTurboFanned = function(f) { return %ActiveTierIsTurbofan(f); };
var topFrameIsInterpreted = function() { return false; };
var topFrameIsBaseline = function() { return false; };
var topFrameIsMaglevved = function() { return false; };
var topFrameIsTurboFanned = function() { return false; };
var willBeMaglevved = function() { return true; };
var willBeTurbofanned = function() { return true; };
var prettyPrinted = function(v) { try { return String(v); } catch (e) { return ""; } };
var deepEquals = function(a, b) { return a == b; };
var promiseTestChain = Promise.resolve();
var MjsUnitAssertionError = function(m) { this.message = m; };
var Test262Error = function(m) {
  if (!(this instanceof Test262Error)) return new Test262Error(m);
  this.message = m || "";
};
Test262Error.prototype.toString = function() { return "Test262Error: " + this.message; };
Test262Error.thrower = __redline_noop;
var $ERROR = __redline_noop;
var $DONE = __redline_noop;
var $DONOTEVALUATE = __redline_noop;
var __consolePrintHandle__ = __redline_noop;
var isNegativeZero = function(v) { return v === 0 && 1 / v === -Infinity; };
var isPrimitive = function(v) { return !v || (typeof v !== "object" && typeof v !== "function"); };
var formatIdentityFreeValue = function(v) { return isPrimitive(v) && typeof v !== "symbol" ? prettyPrinted(v) : undefined; };
var formatSimpleValue = prettyPrinted;
var compareArray = function(a, b) { return !!(a && b && a.length === b.length); };
compareArray.isSameValue = Object.is;
compareArray.format = prettyPrinted;
var assert = function(c) { return !!c; };
assert._isSameValue = function(a, b) { return Object.is(a, b); };
assert.sameValue = function(a, b) { return Object.is(a, b); };
assert.notSameValue = function(a, b) { return !Object.is(a, b); };
assert.throws = function(e, f) { return __redline_call(f); };
assert.compareArray = compareArray;
assert._formatIdentityFreeValue = formatIdentityFreeValue;
assert._toString = formatSimpleValue;
var __redline_realm = function(r, g) {
  var o = {
    evalScript: function(s) {
      var cr = r === undefined && typeof Realm === "object" && Realm.current ? Realm.current() : r;
      return typeof Realm === "object" && Realm.eval && cr !== undefined ? Realm.eval(cr, s) : __redline_call(s);
    },
    createRealm: function() {
      var nr = typeof Realm === "object" && Realm.createAllowCrossRealmAccess ? Realm.createAllowCrossRealmAccess() : undefined;
      var ng = nr !== undefined && typeof Realm === "object" && Realm.global ? Realm.global(nr) : (nr !== undefined && Realm.eval ? Realm.eval(nr, "this") : globalThis);
      return __redline_realm(nr, ng);
    },
    detachArrayBuffer: function(b) { try { %ArrayBufferDetach(b); } catch (e) {} },
    gc: function() { try { gc(); } catch (e) {} },
    global: g || globalThis,
    IsHTMLDDA: __redline_noop
  };
  try { (g || globalThis).$262 = o; } catch (e) {}
  return o;
};
var $262 = __redline_realm(undefined, globalThis);
var load = __redline_noop;
var quit = __redline_noop;
var testRunner = { quit: __redline_noop };
try { if (typeof d8 === "object" && d8.file) d8.file.execute = __redline_noop; } catch (e) {}
)JS";

// True for a byte that a name of the prelude can have: an ASCII letter, an
// ASCII digit, `_` or `$`. Each other byte ends a token of Scan().
constexpr bool IsNameByte(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '$';
}

// IsNameByte() as a table, for the loop of Scan().
constexpr std::array<bool, 256> kNameBytes = [] {
  std::array<bool, 256> table{};
  for (int c = 0; c < 256; c++) {
    table[static_cast<size_t>(c)] = IsNameByte(static_cast<unsigned char>(c));
  }
  return table;
}();

constexpr bool IsWhiteSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}

// True if text[pos...] is `.name` or `?.name` after white space. Then the
// code reads one property by a name that is in the text. `?.` with a digit
// after it is a condition and a number, and `?.[` has a computed key.
bool IsPropertyByName(std::string_view text, size_t pos) {
  const size_t size = text.size();
  while (pos < size && IsWhiteSpace(text[pos])) {
    pos++;
  }
  if (pos < size && text[pos] == '.') {
    return true;
  }
  if (pos + 1 >= size || text[pos] != '?' || text[pos + 1] != '.') {
    return false;
  }
  pos += 2;
  while (pos < size && IsWhiteSpace(text[pos])) {
    pos++;
  }
  if (pos >= size) {
    return false;
  }
  const unsigned char c = static_cast<unsigned char>(text[pos]);
  // A name starts with a letter, `_`, `$`, an escape sequence or a
  // character that is not ASCII. A private name starts with `#`.
  return (IsNameByte(c) && (c < '0' || c > '9')) || c == '#' || c == '\\' ||
         c >= 128;
}

// The value of the hexadecimal digit `c`, or -1.
constexpr int HexDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

// What an escape sequence gives to the token of Scan().
enum class Escape {
  kNameByte,  // a byte for the token
  kNothing,   // a line continuation: the token continues
  kBreak,     // a character that ends the token
};

// Decodes the escape sequence of a string literal or of an identifier at
// text[*pos], which is a backslash. Sets `*pos` to the position after the
// sequence, and `*byte` for kNameByte. A sequence that is not correct ends
// the token.
Escape DecodeEscape(std::string_view text, size_t* pos, char* byte) {
  size_t i = *pos + 1;
  if (i >= text.size()) {
    *pos = i;
    return Escape::kBreak;
  }
  const char escaped = text[i++];
  uint32_t code = 0;
  // Reads `digits` hexadecimal digits at i. False if they are not there.
  auto read_hex = [&](int digits) {
    if (i + static_cast<size_t>(digits) > text.size()) {
      return false;
    }
    for (int d = 0; d < digits; d++) {
      const int value = HexDigit(text[i + static_cast<size_t>(d)]);
      if (value < 0) {
        return false;
      }
      code = code * 16 + static_cast<uint32_t>(value);
    }
    i += static_cast<size_t>(digits);
    return true;
  };
  switch (escaped) {
    case 'u':
      if (i < text.size() && text[i] == '{') {
        size_t j = i + 1;
        while (j < text.size() && HexDigit(text[j]) >= 0) {
          // A large value is not a name byte. Keep it large, without
          // overflow.
          code = std::min<uint32_t>(
              code * 16 + static_cast<uint32_t>(HexDigit(text[j])), 0x110000);
          j++;
        }
        if (j == i + 1 || j >= text.size() || text[j] != '}') {
          *pos = i;
          return Escape::kBreak;
        }
        i = j + 1;
      } else if (!read_hex(4)) {
        *pos = i;
        return Escape::kBreak;
      }
      break;
    case 'x':
      if (!read_hex(2)) {
        *pos = i;
        return Escape::kBreak;
      }
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
      // A legacy octal escape sequence has at most 3 digits and a value of
      // at most 255.
      code = static_cast<uint32_t>(escaped - '0');
      const int max_digits = escaped <= '3' ? 3 : 2;
      for (int d = 1; d < max_digits && i < text.size() && text[i] >= '0' &&
                      text[i] <= '7';
           d++) {
        code = code * 8 + static_cast<uint32_t>(text[i++] - '0');
      }
      break;
    }
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't':
    case 'v':
      *pos = i;
      return Escape::kBreak;
    case '\n':
      *pos = i;
      return Escape::kNothing;
    case '\r':
      *pos = i < text.size() && text[i] == '\n' ? i + 1 : i;
      return Escape::kNothing;
    default:
      // U+2028 and U+2029 after a backslash also continue a line.
      if (escaped == '\xE2' && i + 1 < text.size() && text[i] == '\x80' &&
          (text[i + 1] == '\xA8' || text[i + 1] == '\xA9')) {
        *pos = i + 2;
        return Escape::kNothing;
      }
      // Each other character stands for itself.
      code = static_cast<unsigned char>(escaped);
      break;
  }
  *pos = i;
  if (code < 128 && IsNameByte(static_cast<unsigned char>(code))) {
    *byte = static_cast<char>(code);
    return Escape::kNameByte;
  }
  return Escape::kBreak;
}

// True if no text after `statement` can continue the statement. A statement
// that ends with an expression and has no semicolon can continue in the
// next statement, for example `var a = f` and `(1);`.
bool EndsItself(const fast::Statement* statement, std::string_view source) {
  if (statement->end > statement->start &&
      source[static_cast<size_t>(statement->end) - 1] == ';') {
    return true;
  }
  return statement->kind == fast::NodeKind::kTryStatement;
}

// True if `expression` has no effect. Accepted forms are a name, a function
// expression, a property of a name (for example `Object.is`), or an object
// literal of these.
bool HasNoEffect(const fast::Expression* expression) {
  switch (expression->kind) {
    case fast::NodeKind::kIdentifier:
    case fast::NodeKind::kFunctionExpression:
      return true;
    case fast::NodeKind::kMemberExpression: {
      const fast::Expression* object = expression;
      while (const auto* member = object->As<fast::MemberExpression>()) {
        if (member->computed || member->optional) {
          return false;
        }
        object = member->object;
      }
      return object->Is<fast::Identifier>();
    }
    case fast::NodeKind::kObjectExpression: {
      // A spread element, a computed key or an accessor can run code.
      const std::vector<fast::Node*>& properties =
          expression->Cast<fast::ObjectExpression>()->properties;
      return std::all_of(
          properties.begin(), properties.end(), [](const fast::Node* node) {
            const auto* property = node->As<fast::Property>();
            return property != nullptr && !property->computed &&
                   property->property_kind == fast::PropertyKind::kInit &&
                   property->value != nullptr && HasNoEffect(property->value);
          });
    }
    default:
      return false;
  }
}

// If `statement` is `NAME.key = value` or `NAME.prototype.key = value` and
// the value has no effect, returns the identifier NAME.
const fast::Identifier* ExtendedName(const fast::Statement* statement) {
  const auto* expression = statement->As<fast::ExpressionStatement>();
  if (expression == nullptr || expression->is_directive) {
    return nullptr;
  }
  const auto* assignment =
      expression->expression->As<fast::AssignmentExpression>();
  if (assignment == nullptr || assignment->op != fast::AssignOp::kAssign ||
      !HasNoEffect(assignment->right)) {
    return nullptr;
  }
  const auto* target = assignment->left->As<fast::MemberExpression>();
  if (target == nullptr || target->computed || target->optional ||
      !target->property->Is<fast::Identifier>()) {
    return nullptr;
  }
  const fast::Expression* object = target->object;
  if (const auto* member = object->As<fast::MemberExpression>()) {
    const auto* property = member->property->As<fast::Identifier>();
    if (member->computed || member->optional || property == nullptr ||
        property->name != "prototype") {
      return nullptr;
    }
    object = member->object;
  }
  return object->As<fast::Identifier>();
}

// True if `statement` gives `binding` a new function with a `prototype`
// object, and no code of the prelude assigns to the binding. Then a new
// property of this function or of its prototype is not visible without the
// name of the binding.
bool DeclaresNewValue(const fast::Statement* statement,
                      const fast::Binding* binding) {
  if (binding->assigned || binding->declarations.size() != 1) {
    return false;
  }
  const auto* declaration = statement->As<fast::VariableDeclaration>();
  if (declaration == nullptr) {
    return false;
  }
  for (const fast::VariableDeclarator* declarator : declaration->declarations) {
    if (declarator->target != binding->identifier ||
        declarator->init == nullptr) {
      continue;
    }
    const auto* function = declarator->init->As<fast::FunctionExpression>();
    return function != nullptr &&
           (!function->is_async || function->is_generator);
  }
  return false;
}

// True if `statement` is a `var` declaration whose initializers have no
// effect. Such a statement changes no state that a program can read without
// one of the names.
bool OnlyGivesValues(const fast::Statement* statement) {
  const auto* declaration = statement->As<fast::VariableDeclaration>();
  if (declaration == nullptr ||
      declaration->declaration_kind != fast::VariableKind::kVar) {
    return false;
  }
  const std::vector<fast::VariableDeclarator*>& declarators =
      declaration->declarations;
  return std::all_of(
      declarators.begin(), declarators.end(),
      [](const fast::VariableDeclarator* declarator) {
        return declarator->target->Is<fast::Identifier>() &&
               (declarator->init == nullptr || HasNoEffect(declarator->init));
      });
}

// The declarations of the default prelude that call a function. The calls
// change no state that a program can read without the declared name.
// `Promise.resolve()` only makes a promise, and `__redline_realm()` only
// gives the same `$262` to the global object.
constexpr std::string_view kTrustedDeclarations[] = {"promiseTestChain",
                                                     "$262"};

// True if `prelude` has the text of the default prelude. White space at the
// start and at the end does not count, so a file with this text is also the
// default prelude.
bool IsDefaultPrelude(std::string_view prelude) {
  auto trim = [](std::string_view text) {
    while (!text.empty() && IsWhiteSpace(text.front())) {
      text.remove_prefix(1);
    }
    while (!text.empty() && IsWhiteSpace(text.back())) {
      text.remove_suffix(1);
    }
    return text;
  };
  return trim(prelude) == trim(kDefaultPrelude);
}

}  // namespace

std::string_view DefaultPrelude() { return kDefaultPrelude; }

int PreludeLineCount(std::string_view prelude) {
  if (prelude.empty()) {
    return 0;
  }
  return static_cast<int>(std::count(prelude.begin(), prelude.end(), '\n')) + 1;
}

const char* PreludeFallbackName(PreludeFallback fallback) {
  switch (fallback) {
    case PreludeFallback::kNone:
      return "none";
    case PreludeFallback::kEval:
      return "eval";
    case PreludeFallback::kFunction:
      return "function";
    case PreludeFallback::kGlobalThis:
      return "global_this";
    case PreludeFallback::kRealm:
      return "realm";
    case PreludeFallback::kThis:
      return "this";
  }
  return "unknown";
}

// static
std::unique_ptr<OnDemandPrelude> OnDemandPrelude::Build(
    std::string_view prelude, bool fallback_rules, std::string* error) {
  if (!IsDefaultPrelude(prelude)) {
    *error = "the prelude is not the default prelude";
    return nullptr;
  }
  fast::ParseError parse_error;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(prelude, fast::ParseOptions(), &parse_error);
  if (tree == nullptr) {
    *error = "the prelude does not parse: " + parse_error.ToString();
    return nullptr;
  }
  const std::vector<fast::Statement*>& body = tree->root()->body;
  const size_t count = body.size();
  if (count == 0 || count > kMaxStatements) {
    *error = StringPrintf(
        "the prelude has %zu top-level statements (at least 1, at most %zu)",
        count, kMaxStatements);
    return nullptr;
  }
  std::vector<int32_t> starts;
  starts.reserve(count);
  for (size_t i = 0; i < count; i++) {
    const fast::Statement* statement = body[i];
    if (!EndsItself(statement, prelude)) {
      *error = StringPrintf(
          "top-level statement %zu of the prelude can continue in the next "
          "statement",
          i + 1);
      return nullptr;
    }
    // A top-level string can change strict mode if the statements around it
    // are not there.
    const auto* expression = statement->As<fast::ExpressionStatement>();
    if (expression != nullptr &&
        expression->expression->Is<fast::StringLiteral>()) {
      *error = StringPrintf(
          "top-level statement %zu of the prelude is a string", i + 1);
      return nullptr;
    }
    starts.push_back(statement->start);
  }
  std::unique_ptr<fast::ScopeAnalysis> scopes =
      fast::ScopeAnalysis::Analyze(tree.get());
  const fast::Scope* root = scopes->root();
  for (const fast::Scope* scope : scopes->scopes()) {
    if (scope->var_scope() == root &&
        (scope->has_direct_eval || scope->kind == fast::ScopeKind::kWith)) {
      *error =
          "the top level of the prelude has a direct eval or a with statement";
      return nullptr;
    }
  }
  for (const fast::Binding* binding : scopes->bindings()) {
    // The scope analysis does not model these declarations precisely.
    if (binding->pinned && binding->scope->var_scope() == root) {
      *error = StringPrintf(
          "the scope of `%.*s` at the top level of the prelude is not known",
          static_cast<int>(binding->name.size()), binding->name.data());
      return nullptr;
    }
  }
  // The top-level statement that contains `node`. Each identifier of the
  // tree is in a top-level statement.
  auto statement_of = [&starts](const fast::Node* node) {
    if (node->start < starts.front()) {
      Fatal(
          "OnDemandPrelude::Build: a node at offset %d is in front of the "
          "first statement of the prelude",
          node->start);
    }
    return static_cast<size_t>(
               std::upper_bound(starts.begin(), starts.end(), node->start) -
               starts.begin()) -
           1;
  };
  // The names of the default prelude: the bindings of the script scope.
  std::vector<std::string_view> names;
  absl::flat_hash_map<std::string_view, size_t> name_index;
  auto index_of = [&](std::string_view name) {
    auto [it, inserted] = name_index.try_emplace(name, names.size());
    if (inserted) {
      names.push_back(name);
    }
    return it->second;
  };
  // For each statement: the names that it gives, and the names that it uses.
  std::vector<std::vector<size_t>> gives(count);
  std::vector<std::vector<size_t>> uses(count);
  for (const fast::Binding* binding : root->bindings) {
    const size_t name = index_of(binding->name);
    for (const fast::Identifier* identifier : binding->declarations) {
      gives[statement_of(identifier)].push_back(name);
    }
    for (const fast::Identifier* identifier : binding->references) {
      uses[statement_of(identifier)].push_back(name);
    }
  }
  // A statement that only extends a new function of the prelude belongs to
  // the name of this function.
  std::vector<bool> extends(count, false);
  for (size_t i = 0; i < count; i++) {
    if (!gives[i].empty()) {
      continue;
    }
    const fast::Identifier* extended = ExtendedName(body[i]);
    if (extended == nullptr) {
      continue;
    }
    const fast::Binding* binding = scopes->BindingOf(extended);
    if (binding != nullptr && binding->scope == root &&
        DeclaresNewValue(body[statement_of(binding->identifier)], binding)) {
      gives[i].push_back(index_of(binding->name));
      extends[i] = true;
    }
  }
  // The statements of each name: the statements that give the name, and all
  // statements that these statements use. The loop ends because the sets
  // only grow.
  std::vector<Mask> statements_of(names.size());
  for (size_t i = 0; i < count; i++) {
    for (size_t name : gives[i]) {
      statements_of[name].set(i);
    }
  }
  for (bool changed = true; changed;) {
    changed = false;
    for (size_t i = 0; i < count; i++) {
      Mask used;
      for (size_t name : uses[i]) {
        used |= statements_of[name];
      }
      for (size_t name : gives[i]) {
        if ((statements_of[name] | used) != statements_of[name]) {
          statements_of[name] |= used;
          changed = true;
        }
      }
    }
  }
  // True if statement i goes only with the names that it gives. Each other
  // statement goes in front of each program.
  auto goes_with_names = [&](size_t i) {
    if (gives[i].empty()) {
      return false;
    }
    if (extends[i] || OnlyGivesValues(body[i])) {
      return true;
    }
    return std::all_of(gives[i].begin(), gives[i].end(), [&names](size_t name) {
      return std::find(std::begin(kTrustedDeclarations),
                       std::end(kTrustedDeclarations),
                       names[name]) != std::end(kTrustedDeclarations);
    });
  };
  std::unique_ptr<OnDemandPrelude> result(new OnDemandPrelude());
  result->text_ = std::string(prelude);
  result->fallback_rules_ = fallback_rules;
  for (size_t i = 0; i < count; i++) {
    // The text in front of the first statement goes with the first
    // statement.
    result->offsets_.push_back(i == 0 ? 0 : static_cast<size_t>(starts[i]));
  }
  result->offsets_.push_back(prelude.size());
  for (size_t i = 0; i < count; i++) {
    const std::string_view chunk = prelude.substr(
        result->offsets_[i], result->offsets_[i + 1] - result->offsets_[i]);
    result->newlines_.push_back(
        static_cast<int>(std::count(chunk.begin(), chunk.end(), '\n')));
    // A statement that gives no name can change global state. A statement
    // that gives a name and can have one more effect can do the same.
    if (!goes_with_names(i)) {
      result->always_.set(i);
      for (size_t name : uses[i]) {
        result->always_ |= statements_of[name];
      }
      for (size_t name : gives[i]) {
        result->always_ |= statements_of[name];
      }
    }
  }
  for (size_t name = 0; name < names.size(); name++) {
    const std::string_view text = names[name];
    if (std::all_of(text.begin(), text.end(), [](char c) {
          return IsNameByte(static_cast<unsigned char>(c));
        })) {
      result->tokens_[std::string(text)].statements = statements_of[name];
    } else {
      // Scan() cannot find this name.
      result->always_ |= statements_of[name];
    }
  }
  if (result->always_.none()) {
    *error = "the prelude keeps no statement in front of each program";
    return nullptr;
  }
  if (fallback_rules) {
    result->tokens_["eval"].fallback = PreludeFallback::kEval;
    result->tokens_["Function"].fallback = PreludeFallback::kFunction;
    result->tokens_["globalThis"].fallback = PreludeFallback::kGlobalThis;
    result->tokens_["Realm"].fallback = PreludeFallback::kRealm;
    result->tokens_["this"].fallback = PreludeFallback::kThis;
  }
  for (const auto& [token, unused] : result->tokens_) {
    result->lengths_[static_cast<unsigned char>(token[0])] |=
        uint64_t{1} << std::min<size_t>(token.size(), 63);
  }
  Mask all;
  for (size_t i = 0; i < count; i++) {
    all.set(i);
  }
  result->whole_ = result->MakeSelection(all);
  return result;
}

void OnDemandPrelude::AddHelper(std::string_view helper) {
  Helper added;
  added.text = std::string(helper);
  Scan(helper, /*rules=*/false, &added.statements);
  helpers_.push_back(std::move(added));
}

PreludeFallback OnDemandPrelude::Scan(std::string_view text, bool rules,
                                      Mask* statements) const {
  const size_t size = text.size();
  // Adds the statements of `token`, which ends in front of text[end].
  // Returns the fallback rule that applies to the token.
  auto look_up = [&](std::string_view token, size_t end) {
    if (token.empty() || ((lengths_[static_cast<unsigned char>(token[0])] >>
                           std::min<size_t>(token.size(), 63)) &
                          1) == 0) {
      return PreludeFallback::kNone;
    }
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
      return PreludeFallback::kNone;
    }
    *statements |= it->second.statements;
    const PreludeFallback fallback = it->second.fallback;
    if (!rules ||
        (fallback == PreludeFallback::kThis && IsPropertyByName(text, end))) {
      return PreludeFallback::kNone;
    }
    return fallback;
  };
  // A token that has escape sequences, without them.
  std::string decoded;
  size_t i = 0;
  while (i < size) {
    if (!kNameBytes[static_cast<unsigned char>(text[i])] && text[i] != '\\') {
      i++;
      continue;
    }
    const size_t start = i;
    while (i < size && kNameBytes[static_cast<unsigned char>(text[i])]) {
      i++;
    }
    std::string_view token = text.substr(start, i - start);
    // The part of `token` after its last line continuation.
    std::string_view last_line;
    if (i < size && text[i] == '\\') {
      // In a string literal, `\x66ail` is `fail`, and `\nfail` is a new line
      // and `fail`. In an identifier, `f\u0061il` is `fail`.
      decoded.assign(token);
      size_t continued = std::string::npos;
      while (i < size) {
        if (kNameBytes[static_cast<unsigned char>(text[i])]) {
          decoded.push_back(text[i++]);
          continue;
        }
        if (text[i] != '\\') {
          break;
        }
        char byte = 0;
        const Escape escape = DecodeEscape(text, &i, &byte);
        if (escape == Escape::kBreak) {
          break;
        }
        if (escape == Escape::kNameByte) {
          decoded.push_back(byte);
        } else {
          continued = decoded.size();
        }
      }
      token = decoded;
      // A backslash in front of a line break continues a string literal,
      // but it does not continue a line comment. After the comment, the
      // next line is code. The text does not tell which case it is. Thus
      // the search also looks up the part after the last line break.
      if (continued != std::string::npos) {
        last_line = token.substr(continued);
      }
    }
    for (std::string_view candidate : {token, last_line}) {
      const PreludeFallback fallback = look_up(candidate, i);
      if (fallback != PreludeFallback::kNone) {
        return fallback;
      }
    }
  }
  return PreludeFallback::kNone;
}

OnDemandPrelude::Selection OnDemandPrelude::MakeSelection(
    const Mask& statements) const {
  Selection selection;
  selection.statements = statements;
  for (size_t i = 0; i < newlines_.size(); i++) {
    if (statements[i]) {
      selection.count++;
      selection.bytes += offsets_[i + 1] - offsets_[i];
      selection.lines += newlines_[i];
    }
  }
  if (selection.count == 0) {
    Fatal("OnDemandPrelude::MakeSelection: empty selection");
  }
  // The new line between the text and the program.
  selection.lines++;
  return selection;
}

OnDemandPrelude::Selection OnDemandPrelude::Select(
    std::string_view program) const {
  Mask statements = always_;
  for (const Helper& helper : helpers_) {
    if (program.starts_with(helper.text)) {
      statements |= helper.statements;
      program.remove_prefix(helper.text.size());
      break;
    }
  }
  const PreludeFallback fallback = Scan(program, /*rules=*/true, &statements);
  if (fallback != PreludeFallback::kNone) {
    Selection whole = whole_;
    whole.fallback = fallback;
    return whole;
  }
  return MakeSelection(statements);
}

void OnDemandPrelude::AppendText(const Selection& selection,
                                 std::string* out) const {
  if (selection.count == 0) {
    Fatal("OnDemandPrelude::AppendText: empty selection");
  }
  for (size_t i = 0; i < newlines_.size(); i++) {
    if (selection.statements[i]) {
      out->append(text_, offsets_[i], offsets_[i + 1] - offsets_[i]);
    }
  }
}

std::string OnDemandPrelude::Text(const Selection& selection) const {
  std::string text;
  text.reserve(selection.bytes);
  AppendText(selection, &text);
  return text;
}

std::unique_ptr<OnDemandPrelude> MakeOnDemandPrelude(std::string_view prelude) {
  const int mode = kPreludeOnDemand.AsInt();
  if (mode == 0 || prelude.empty()) {
    return nullptr;
  }
  std::string error;
  std::unique_ptr<OnDemandPrelude> on_demand =
      OnDemandPrelude::Build(prelude, /*fallback_rules=*/mode == 1, &error);
  if (on_demand == nullptr) {
    REDLINE_WARN(kLog,
                 "prelude.on_demand has no effect, each program gets the "
                 "whole prelude: %s",
                 error.c_str());
    return nullptr;
  }
  // The runtime of a runtime-assisted mutator stands in front of the
  // program, and the fallback rules do not apply to it.
  const std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  for (const std::unique_ptr<Mutator>& mutator : mutators->all()) {
    if (mutator->runtime_assisted()) {
      on_demand->AddHelper(
          static_cast<const RuntimeAssistedMutator&>(*mutator).runtime());
    }
  }
  REDLINE_LOG(kLog,
              "on demand: %zu top-level statements, %zu of them in front of "
              "each program",
              on_demand->statements(), on_demand->always_kept());
  return on_demand;
}

std::string ScriptWithPrelude(std::string_view prelude,
                              const OnDemandPrelude* on_demand,
                              std::string_view program,
                              OnDemandPrelude::Selection* selection) {
  if (prelude.empty()) {
    return std::string(program);
  }
  std::string script;
  if (on_demand == nullptr) {
    script.reserve(prelude.size() + 1 + program.size());
    script.append(prelude).append("\n").append(program);
    return script;
  }
  const OnDemandPrelude::Selection selected = on_demand->Select(program);
  script.reserve(selected.bytes + 1 + program.size());
  on_demand->AppendText(selected, &script);
  script.append("\n").append(program);
  if (selection != nullptr) {
    *selection = selected;
  }
  return script;
}

// static
std::unique_ptr<ScriptTree> ScriptTree::Parse(std::string_view prelude,
                                              std::string_view program,
                                              std::string* error) {
  fast::ParseError parse_error;
  size_t prelude_count = 0;
  if (!prelude.empty()) {
    std::unique_ptr<fast::Tree> alone =
        fast::Parser::ParseScript(prelude, fast::ParseOptions(), &parse_error);
    if (alone == nullptr) {
      *error = "the prelude does not parse: " + parse_error.ToString();
      return nullptr;
    }
    prelude_count = alone->root()->body.size();
  }
  std::unique_ptr<fast::Tree> alone =
      fast::Parser::ParseScript(program, fast::ParseOptions(), &parse_error);
  if (alone == nullptr) {
    *error = "the program does not parse: " + parse_error.ToString();
    return nullptr;
  }
  const size_t program_count = alone->root()->body.size();
  std::unique_ptr<ScriptTree> script(new ScriptTree());
  if (prelude.empty()) {
    script->tree_ = std::move(alone);
  } else {
    std::string text;
    text.reserve(prelude.size() + 1 + program.size());
    text.append(prelude).append("\n").append(program);
    script->tree_ =
        fast::Parser::ParseScript(text, fast::ParseOptions(), &parse_error);
    if (script->tree_ == nullptr) {
      *error = "the script does not parse: " + parse_error.ToString();
      return nullptr;
    }
  }
  const std::vector<fast::Statement*>& body = script->tree_->root()->body;
  // For example, a prelude that ends without a semicolon can continue in
  // the first statement of the program.
  if (body.size() != prelude_count + program_count) {
    *error = StringPrintf(
        "the script has %zu top-level statements, not %zu of the prelude and "
        "%zu of the program",
        body.size(), prelude_count, program_count);
    return nullptr;
  }
  for (size_t i = 0; i < prelude_count; i++) {
    fast::Walk(body[i], [&](fast::Node* node) {
      script->first_program_id_ =
          std::max(script->first_program_id_, node->id + 1);
      return true;
    });
  }
  script->prelude_statements_ = prelude_count;
  return script;
}

size_t ScriptTree::PreludeStatementsNow() const {
  const std::vector<fast::Statement*>& body = tree_->root()->body;
  return static_cast<size_t>(std::count_if(
      body.begin(), body.end(),
      [this](const fast::Statement* s) { return FromPrelude(s); }));
}

size_t ScriptTree::ProgramNodes() const {
  size_t nodes = 1;  // the root
  for (const fast::Statement* statement : tree_->root()->body) {
    if (!FromPrelude(statement)) {
      nodes += fast::CountNodes(statement);
    }
  }
  return nodes;
}

std::string ScriptTree::PrintProgram() {
  // Print the root with only the program statements, then restore it.
  std::vector<fast::Statement*>& body = tree_->root()->body;
  std::vector<fast::Statement*> all = body;
  body.erase(std::remove_if(
                 body.begin(), body.end(),
                 [this](const fast::Statement* s) { return FromPrelude(s); }),
             body.end());
  std::string text = fast::Print(tree_->root());
  body = std::move(all);
  return text;
}

}  // namespace redline
