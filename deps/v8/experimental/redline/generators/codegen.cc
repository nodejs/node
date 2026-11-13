// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/codegen.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "experimental/redline/core/tunable.h"
#include "experimental/redline/generators/members.h"
#include "experimental/redline/generators/shapes_generators.h"
#include "experimental/redline/generators/values_generators.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

bool IsIdentifierStart(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
         c == '$';
}

bool IsIdentifierPart(char c) {
  return IsIdentifierStart(c) || (c >= '0' && c <= '9');
}

// Types for which a literal can be synthesized directly.
JsType RandomLiteralType(Random& rng) {
  static const JsType kTypes[] = {
      JsType::kNumber,  JsType::kNumber,  JsType::kNumber, JsType::kString,
      JsType::kString,  JsType::kBoolean, JsType::kBigInt, JsType::kRegExp,
      JsType::kNullish, JsType::kArray,   JsType::kObject};
  return kTypes[rng.Index(sizeof(kTypes) / sizeof(kTypes[0]))];
}

}  // namespace

bool IsIdentifierName(std::string_view name) {
  if (name.empty() || !IsIdentifierStart(name[0])) {
    return false;
  }
  for (char c : name) {
    if (!IsIdentifierPart(c)) {
      return false;
    }
  }
  return true;
}

// --- GenContext
// ----------------------------------------------------------------

GenContext GenContext::Nested(bool is_function, bool is_arrow, bool is_async,
                              bool is_generator) const {
  GenContext nested(builder, generators, env);
  nested.slot = slot;
  nested.syntax = syntax;
  nested.visible = visible;
  nested.depth = depth + 1;
  nested.max_depth = max_depth;
  nested.expr_depth = expr_depth;
  nested.out = nullptr;
  if (is_function) {
    nested.syntax.in_function = true;
    nested.syntax.in_async = is_async;
    nested.syntax.in_generator = is_generator;
    nested.syntax.in_loop = false;
    nested.syntax.in_switch = false;
    nested.syntax.labels.clear();
    nested.syntax.loop_labels.clear();
    if (!is_arrow) {
      nested.syntax.in_method = false;
      nested.syntax.in_constructor = false;
      nested.syntax.has_super_class = false;
      nested.syntax.in_static_block = false;
      nested.syntax.in_class_field = false;
      nested.syntax.static_member = false;
      nested.syntax.class_this = false;
      nested.syntax.has_this = true;
      nested.syntax.has_arguments = true;
    }
  }
  return nested;
}

GenContext GenContext::Inner(bool in_loop, bool in_switch) const {
  GenContext inner(builder, generators, env);
  inner.slot = slot;
  inner.syntax = syntax;
  inner.visible = visible;
  inner.depth = depth + 1;
  inner.max_depth = max_depth;
  inner.expr_depth = expr_depth;
  inner.out = nullptr;
  inner.cannot_throw = cannot_throw;  // the inner block runs here too
  if (in_loop) {
    inner.syntax.in_loop = true;
  }
  if (in_switch) {
    inner.syntax.in_switch = true;
  }
  return inner;
}

// --- leaves
// --------------------------------------------------------------------

REDLINE_TUNABLE(kLiteralSmallIntMax, "codegen.literal.small_int_max", 10, 0,
                1024,
                "largest value of a small random integer literal (0..max)");
REDLINE_TUNABLE(kLiteralMediumIntMin, "codegen.literal.medium_int_min", -5,
                -1024, 1024,
                "smallest value of a medium random integer literal");
REDLINE_TUNABLE(kLiteralMediumIntMax, "codegen.literal.medium_int_max", 100,
                -1024, 1024,
                "largest value of a medium random integer literal");
REDLINE_TUNABLE(kLiteralLargeIntMax, "codegen.literal.large_int_max", 1 << 16,
                0, 1 << 30,
                "largest value of a large random integer literal (0..max)");

fast::Expression* CodeGen::Integer() {
  Random& r = rng();
  switch (r.Index(4)) {
    case 0:
      return mk::Num(
          tree(), static_cast<double>(r.Range(0, kLiteralSmallIntMax.AsInt())));
    case 1:
      return mk::Num(
          tree(), static_cast<double>(r.Range(kLiteralMediumIntMin.AsInt(),
                                              kLiteralMediumIntMax.AsInt())));
    case 2: {
      double v = r.Pick(env().numbers());
      if (v != v || v != static_cast<double>(static_cast<int64_t>(v))) {
        v = 1;
      }
      return mk::Num(tree(), v);
    }
    default:
      return mk::Num(
          tree(), static_cast<double>(r.Range(0, kLiteralLargeIntMax.AsInt())));
  }
}

REDLINE_TUNABLE(
    kLiteralFractionMin, "codegen.literal.fraction_min", -1000, -1048576,
    1048576,
    "smallest numerator of a random fractional number literal (numerator / 8)");
REDLINE_TUNABLE(
    kLiteralFractionMax, "codegen.literal.fraction_max", 1000, -1048576,
    1048576,
    "largest numerator of a random fractional number literal (numerator / 8)");

fast::Expression* CodeGen::Number() {
  Random& r = rng();
  switch (r.Index(5)) {
    case 0:
    case 1:
      return Integer();
    case 2:
      return mk::Num(tree(), r.Pick(env().numbers()));
    case 3:
      return mk::Num(tree(), r.Double() * 10.0 - 5.0);
    default:
      return mk::Num(tree(),
                     static_cast<double>(r.Range(kLiteralFractionMin.AsInt(),
                                                 kLiteralFractionMax.AsInt())) /
                         8.0);
  }
}

REDLINE_TUNABLE(kLiteralRandomStringChance,
                "codegen.literal.random_string_chance", 0.15, 0, 1,
                "probability that a string literal is a short random ASCII "
                "string instead of a corpus string");
REDLINE_TUNABLE(kLiteralRandomStringMaxLength,
                "codegen.literal.random_string_max_length", 6, 0, 64,
                "most characters in a random ASCII string literal");
REDLINE_TUNABLE(kLiteralPropertyNameStringChance,
                "codegen.literal.property_name_string_chance", 0.1, 0, 1,
                "probability that a string literal is a property name (useful "
                "for computed access)");

fast::Expression* CodeGen::String() {
  Random& r = rng();
  if (r.Chance(kLiteralRandomStringChance())) {
    // Short random ASCII string.
    std::string s;
    int n = static_cast<int>(r.Range(0, kLiteralRandomStringMaxLength.AsInt()));
    for (int i = 0; i < n; i++) {
      s.push_back(static_cast<char>('a' + r.Index(26)));
    }
    return mk::Str(tree(), s);
  }
  if (r.Chance(kLiteralPropertyNameStringChance())) {
    // Property name as string (useful for computed access).
    return mk::Str(tree(), PropertyName());
  }
  return mk::Str(tree(), r.Pick(env().strings()));
}

fast::Expression* CodeGen::Boolean() { return mk::Bool(tree(), rng().Coin()); }

REDLINE_TUNABLE(kLiteralBigintTableChance,
                "codegen.literal.bigint_table_chance", 0.6, 0, 1,
                "probability that a BigInt literal comes from the table of "
                "interesting values instead of being random");
REDLINE_TUNABLE(kLiteralRandomBigintMax, "codegen.literal.random_bigint_max",
                1000000, 0, 1 << 30, "largest random BigInt literal (0..max)");

fast::Expression* CodeGen::BigInt() {
  Random& r = rng();
  static const char* const kBig[] = {"0",
                                     "1",
                                     "2",
                                     "255",
                                     "256",
                                     "4294967295",
                                     "9007199254740993",
                                     "18446744073709551615",
                                     "18446744073709551616",
                                     "340282366920938463463374607431768211456"};
  if (r.Chance(kLiteralBigintTableChance())) {
    return mk::BigInt(tree(), kBig[r.Index(sizeof(kBig) / sizeof(kBig[0]))]);
  }
  return mk::BigInt(
      tree(), std::to_string(r.Range(0, kLiteralRandomBigintMax.AsInt())));
}

REDLINE_TUNABLE(kRegExpGeneratedChance, "codegen.regexp.generated_chance", 0.75,
                0, 1,
                "probability that a regexp literal gets a generated pattern "
                "rather than one of the environment's fixed ones");
REDLINE_TUNABLE(kRegExpMaxTerms, "codegen.regexp.max_terms", 4, 1, 16,
                "most terms in one alternative of a pattern");
REDLINE_TUNABLE(kRegExpMaxDepth, "codegen.regexp.max_depth", 2, 0, 8,
                "deepest group nesting in a pattern");
REDLINE_TUNABLE(kRegExpFlagChance, "codegen.regexp.flag_chance", 0.25, 0, 1,
                "probability of each of the flags d, g, i, m, s, y and of one "
                "unicode flag (u or v)");

namespace {

// A random regular expression pattern from general building blocks: atoms,
// escapes, character classes, groups, lookarounds, modifiers, quantifiers,
// alternatives and backreferences. Valid by construction for its mode
// (`unicode`: the u or v flag, `sets`: the v flag).
class RegExpPatternBuilder {
 public:
  RegExpPatternBuilder(Random& r, bool unicode, bool sets)
      : r_(r), unicode_(unicode), sets_(sets) {}

  std::string Build() {
    std::string out;
    Disjunction(0, &out);
    return out;
  }

 private:
  void Disjunction(int depth, std::string* out) {
    int alternatives = r_.Chance(0.25) ? 2 + static_cast<int>(r_.Index(2)) : 1;
    for (int i = 0; i < alternatives; i++) {
      if (i > 0) {
        *out += '|';
      }
      int terms = 1 + static_cast<int>(r_.Index(
                          static_cast<size_t>(kRegExpMaxTerms.AsInt())));
      for (int k = 0; k < terms; k++) {
        Term(depth, out);
      }
    }
  }

  void Term(int depth, std::string* out) {
    bool nest = depth < kRegExpMaxDepth.AsInt();
    switch (r_.Index(nest ? 9 : 6)) {
      case 0:
      case 1:
        Atom(out);
        Quantifier(out);
        break;
      case 2:
        Class(out);
        Quantifier(out);
        break;
      case 3:
        *out += r_.Pick({"^", "$", "\\b", "\\B"});
        break;
      case 4:
        Backreference(out);
        break;
      case 5:
        *out += '.';
        Quantifier(out);
        break;
      case 6:
        Group(depth, out);
        Quantifier(out);
        break;
      case 7:
        // Never quantified: an error for lookbehinds, and for all
        // lookarounds in unicode mode.
        *out += r_.Pick({"(?=", "(?!", "(?<=", "(?<!"});
        Disjunction(depth + 1, out);
        *out += ')';
        break;
      default:
        *out += r_.Pick({"(?i:", "(?m:", "(?s:", "(?-i:", "(?i-s:", "(?ms:"});
        Disjunction(depth + 1, out);
        *out += ')';
        Quantifier(out);
        break;
    }
  }

  void Atom(std::string* out) {
    switch (r_.Index(4)) {
      case 0:
        *out += r_.Pick({"a", "b", "x", "Z", "0", "7", "_", " ", ",", "-", ":",
                         "\u00e9", "\u4e2d"});
        break;
      case 1:
        *out += r_.Pick({"\\d", "\\D", "\\w", "\\W", "\\s", "\\S", "\\n", "\\t",
                         "\\.", "\\/", "\\\\"});
        break;
      case 2:
        *out += unicode_ ? r_.Pick({"\\u{1F600}", "\\u0041", "\\x7f", "\\cJ"})
                         : r_.Pick({"\\u0041", "\\x7f", "\\cJ"});
        break;
      default:
        if (unicode_) {
          *out += r_.Pick({"\\p{L}", "\\P{L}", "\\p{Lu}", "\\p{Nd}",
                           "\\p{Script=Greek}", "\\p{ASCII}"});
        } else {
          *out += r_.Pick({"ab", "foo", "\\d\\d", "xyz"});
        }
        break;
    }
  }

  void Class(std::string* out) {
    if (sets_ && r_.Chance(0.4)) {
      *out += r_.Pick({"[\\p{L}--[a-z]]", "[[a-z]&&[a-f]]", "[\\q{abc|d}]",
                       "[\\p{ASCII}--\\p{L}]", "[[0-9][a-c]]"});
      return;
    }
    *out += r_.Pick({"[a-z]", "[^a-z]", "[0-9a-f]", "[\\d]", "[\\s\\S]",
                     "[A-Z_]", "[^\\w]", "[xyz]"});
  }

  void Group(int depth, std::string* out) {
    switch (r_.Index(3)) {
      case 0:
        *out += '(';
        groups_++;
        break;
      case 1:
        *out += "(?:";
        break;
      default:
        names_++;
        groups_++;
        *out += "(?<g" + std::to_string(names_) + ">";
        break;
    }
    Disjunction(depth + 1, out);
    *out += ')';
  }

  void Backreference(std::string* out) {
    if (names_ > 0 && r_.Coin()) {
      *out += "\\k<g" +
              std::to_string(1 + r_.Index(static_cast<size_t>(names_))) + ">";
    } else if (groups_ > 0) {
      // In a group, so that a following digit does not extend the number.
      *out += "(?:\\" +
              std::to_string(1 + r_.Index(static_cast<size_t>(groups_))) + ")";
    } else {
      Atom(out);
    }
  }

  void Quantifier(std::string* out) {
    if (!r_.Chance(0.4)) {
      return;
    }
    *out += r_.Pick({"*", "+", "?", "{2}", "{1,3}", "{0,}"});
    if (r_.Chance(0.3)) {
      *out += '?';
    }
  }

  Random& r_;
  bool unicode_;
  bool sets_;
  int groups_ = 0;
  int names_ = 0;
};

}  // namespace

fast::Expression* CodeGen::RegExp() {
  Random& r = rng();
  if (!r.Chance(kRegExpGeneratedChance())) {
    const auto& re = r.Pick(env().regexps());
    return mk::RegExp(tree(), re.first, re.second);
  }
  std::string flags;
  for (char flag : {'d', 'g', 'i', 'm', 's', 'y'}) {
    if (r.Chance(kRegExpFlagChance())) {
      flags += flag;
    }
  }
  bool unicode = r.Chance(kRegExpFlagChance());
  bool sets = unicode && r.Coin();
  if (unicode) {
    flags += sets ? 'v' : 'u';
  }
  RegExpPatternBuilder builder(r, unicode, sets);
  return mk::RegExp(tree(), builder.Build(), flags);
}

fast::Expression* CodeGen::Nullish() {
  return rng().Coin() ? static_cast<fast::Expression*>(mk::Null(tree()))
                      : static_cast<fast::Expression*>(mk::Undefined(tree()));
}

REDLINE_TUNABLE(kCodeGenMaxExprDepth, "codegen.max_expr_depth", 24, 2, 64,
                "deepest nesting of expressions, literals and call arguments "
                "across CodeGen helpers");

bool CodeGen::TooDeep() const {
  return ctx_.expr_depth > kCodeGenMaxExprDepth.AsInt();
}

fast::Expression* CodeGen::Leaf(JsType type) {
  if (fast::Identifier* id = Variable(type)) {
    return id;
  }
  switch (type) {
    case JsType::kString:
      return String();
    case JsType::kBoolean:
      return Boolean();
    case JsType::kBigInt:
      return BigInt();
    case JsType::kRegExp:
      return RegExp();
    case JsType::kNullish:
      return Nullish();
    case JsType::kArray:
      return mk::Array(tree(), {});
    case JsType::kObject:
      return mk::Object(tree(), {});
    case JsType::kFunction:
      return mk::ArrowExpr(tree(), {}, Number());
    case JsType::kSymbol:
      return mk::Call(tree(), mk::Id(tree(), "Symbol"), {String()});
    default:
      return Number();
  }
}

fast::Expression* CodeGen::Literal(JsType type) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return Leaf(type);
  }
  if (type == JsType::kUnknown) {
    type = RandomLiteralType(rng());
  }
  switch (type) {
    case JsType::kNumber:
      return Number();
    case JsType::kString:
      return String();
    case JsType::kBoolean:
      return Boolean();
    case JsType::kBigInt:
      return BigInt();
    case JsType::kRegExp:
      return RegExp();
    case JsType::kNullish:
      return Nullish();
    case JsType::kArray:
      return ArrayLiteral(1);
    case JsType::kObject:
      return ObjectLiteral(1);
    case JsType::kFunction:
      return FunctionValue(ctx_.max_depth);  // leaf-ish: no nested statements
    case JsType::kSymbol:
      return mk::Call(tree(), mk::Id(tree(), "Symbol"), {String()});
    default: {
      // Types that need a constructor.
      if (fast::Expression* e = Construct(type)) {
        return e;
      }
      return Number();
    }
  }
}

REDLINE_TUNABLE(kVariableRecentChance, "codegen.variable.recent_chance", 0.5, 0,
                1,
                "probability that a variable reference is drawn from the first "
                "few candidates only (recently declared ones)");
REDLINE_TUNABLE(kVariableRecentWindow, "codegen.variable.recent_window", 4, 1,
                64,
                "number of leading candidates considered when preferring "
                "recently declared variables");

fast::Identifier* CodeGen::Variable(JsType type) {
  std::vector<const redline::Variable*> candidates;
  for (const redline::Variable& v : ctx_.visible) {
    if (v.name == "arguments" && !ctx_.syntax.has_arguments) {
      continue;
    }
    if (type == JsType::kUnknown || v.type == type) {
      candidates.push_back(&v);
    }
  }
  if (candidates.empty() && type != JsType::kUnknown) {
    // Fall back to variables of unknown type, which may well have the
    // requested type at runtime.
    for (const redline::Variable& v : ctx_.visible) {
      if (v.type == JsType::kUnknown && v.name != "arguments") {
        candidates.push_back(&v);
      }
    }
  }
  if (candidates.empty()) {
    return nullptr;
  }
  // Prefer recently declared (innermost) variables slightly.
  size_t index = rng().Chance(kVariableRecentChance())
                     ? rng().Index(std::min<size_t>(
                           candidates.size(),
                           static_cast<size_t>(kVariableRecentWindow.AsInt())))
                     : rng().Index(candidates.size());
  return mk::Id(tree(), candidates[index]->name);
}

fast::Identifier* CodeGen::MutableVariable(JsType type) {
  std::vector<const redline::Variable*> candidates;
  for (const redline::Variable& v : ctx_.visible) {
    if (v.is_constant) {
      continue;
    }
    if (v.name == "arguments" || v.name == "eval") {
      continue;
    }
    if (type == JsType::kUnknown || v.type == type) {
      candidates.push_back(&v);
    }
  }
  if (candidates.empty()) {
    return nullptr;
  }
  return mk::Id(tree(), rng().Pick(candidates)->name);
}

bool CodeGen::HasVariable(JsType type) {
  for (const redline::Variable& v : ctx_.visible) {
    if (type == JsType::kUnknown || v.type == type) {
      return true;
    }
  }
  return false;
}

// --- composite values
// ----------------------------------------------------------

REDLINE_TUNABLE(kValueVariableChance, "codegen.value.variable_chance", 0.6, 0,
                1,
                "probability that a value is an existing visible variable "
                "rather than a fresh expression");
REDLINE_TUNABLE(kValueCompoundMaxDepth, "codegen.value.compound_max_depth", 2,
                0, 16,
                "values nested less deeply than this may be compound "
                "expressions (arithmetic, comparison, concatenation)");
REDLINE_TUNABLE(kValueCompoundChance, "codegen.value.compound_chance", 0.2, 0,
                1,
                "probability that a shallow value is a compound expression "
                "instead of a literal");
REDLINE_TUNABLE(kValueConstructChance, "codegen.value.construct_chance", 0.5, 0,
                1,
                "probability that a value of an object-like type is a fresh "
                "`new` expression rather than a literal");

fast::Expression* CodeGen::Value(JsType type, int depth) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return Leaf(type);
  }
  Random& r = rng();
  // Variables first: reusing existing values creates data flow.
  if (r.Chance(kValueVariableChance())) {
    if (fast::Identifier* id = Variable(type)) {
      return id;
    }
  }
  // Rarely a special value: a hole NaN, a thin string, a Wasm struct.
  if (fast::Expression* special = MaybeSpecialValue(*this, type)) {
    return special;
  }
  if (ctx_.cannot_throw) {
    // Only literals: operators, calls and constructors can throw. A type
    // that needs a constructor gets a literal of another type.
    switch (type) {
      case JsType::kUnknown:
      case JsType::kNumber:
      case JsType::kString:
      case JsType::kBoolean:
      case JsType::kBigInt:
      case JsType::kRegExp:
      case JsType::kNullish:
      case JsType::kArray:
      case JsType::kObject:
      case JsType::kFunction:
      case JsType::kSymbol:
        return Literal(type);
      default:
        return Literal(JsType::kUnknown);
    }
  }
  if (depth < kValueCompoundMaxDepth.AsInt() &&
      r.Chance(kValueCompoundChance())) {
    switch (type) {
      case JsType::kNumber:
        return BinaryOperation(depth + 1);
      case JsType::kBoolean:
        return Comparison(depth + 1);
      case JsType::kString:
        if (fast::Identifier* id = Variable(JsType::kString)) {
          return mk::Binary(tree(), BinaryOp::kAdd, id, String());
        }
        break;
      case JsType::kUnknown:
        return Expression(depth + 1);
      default:
        break;
    }
  }
  if (type != JsType::kUnknown && type != JsType::kNumber &&
      type != JsType::kString && type != JsType::kBoolean &&
      type != JsType::kBigInt && type != JsType::kNullish &&
      type != JsType::kRegExp && r.Chance(kValueConstructChance())) {
    if (fast::Expression* e = Construct(type)) {
      return e;
    }
  }
  return Literal(type);
}

REDLINE_TUNABLE(kExpressionMaxDepth, "codegen.expression.max_depth", 3, 0, 16,
                "expression nesting depth at which Expression() stops nesting "
                "and produces a plain value");

fast::Expression* CodeGen::Expression(int depth) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return Leaf(JsType::kUnknown);
  }
  Random& r = rng();
  if (depth >= kExpressionMaxDepth.AsInt()) {
    return Value(JsType::kUnknown, depth);
  }
  switch (r.Index(12)) {
    case 0:
      return BinaryOperation(depth + 1);
    case 1:
      return Comparison(depth + 1);
    case 2:
      return UnaryOperation(depth + 1);
    case 3:
      return BuiltinCall();
    case 4: {
      fast::Expression* object = ObjectLike();
      return MethodCall(object, Builder::GuessType(object));
    }
    case 5:
      return Property(ObjectLike());
    case 6:
      if (fast::Expression* e = Construct()) {
        return e;
      }
      return Leaf(JsType::kObject);
    case 7:
      return CallWithArguments(Callable());
    case 8:
      return mk::Cond(tree(), Condition(depth + 1),
                      Value(JsType::kUnknown, depth + 1),
                      Value(JsType::kUnknown, depth + 1));
    case 9:
      return mk::Logical(tree(), RandomLogicalOp(r),
                         Value(JsType::kUnknown, depth + 1),
                         Value(JsType::kUnknown, depth + 1));
    case 10:
      return mk::Unary(tree(), UnaryOp::kTypeof,
                       Value(JsType::kUnknown, depth + 1));
    default:
      return Literal();
  }
}

REDLINE_TUNABLE(kObjectMaxFields, "codegen.object.max_fields", 5, 0, 64,
                "most properties an object literal gets");
REDLINE_TUNABLE(kObjectFieldContinueChance,
                "codegen.object.field_continue_chance", 0.6, 0, 1,
                "probability of adding one more property to an object literal "
                "(geometric count)");
REDLINE_TUNABLE(kObjectMethodMaxDepth, "codegen.object.method_max_depth", 2, 0,
                16,
                "object literals nested less deeply than this may contain "
                "methods and accessors");
REDLINE_TUNABLE(
    kObjectMethodChance, "codegen.object.method_chance", 0.15, 0, 1,
    "probability that an object literal property is a method or accessor");
REDLINE_TUNABLE(kObjectMethodMaxExtraStatements,
                "codegen.object.method_max_extra_statements", 2, 0, 64,
                "most statements an object-literal method body gets beyond its "
                "first one (geometric count)");
REDLINE_TUNABLE(kObjectGetterChance, "codegen.object.getter_chance", 0.3, 0, 1,
                "probability that an object-literal method is a getter instead "
                "of a plain method");
REDLINE_TUNABLE(kObjectSpreadChance, "codegen.object.spread_chance", 0.1, 0, 1,
                "probability that an object literal property is a spread of a "
                "visible object variable");
REDLINE_TUNABLE(kObjectLiteralKeyChance, "codegen.object.literal_key_chance",
                0.1, 0, 1,
                "probability that an object literal property has a numeric, "
                "string or bigint literal key");
REDLINE_TUNABLE(kObjectProtoChance, "codegen.object.proto_chance", 0.1, 0, 1,
                "probability that an object literal property is a __proto__ "
                "entry with an object value (at most one)");
REDLINE_TUNABLE(kObjectVariableKeyChance, "codegen.object.variable_key_chance",
                0.1, 0, 1,
                "probability that an object literal property has a visible "
                "variable as computed key");

fast::Expression* CodeGen::ObjectLiteral(int depth) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return mk::Object(tree(), {});
  }
  Random& r = rng();
  std::vector<Node*> properties;
  int n = r.SmallCount(kObjectMaxFields.AsInt(), kObjectFieldContinueChance());
  bool has_proto = false;
  for (int i = 0; i < n; i++) {
    if (!has_proto && r.Chance(kObjectProtoChance())) {
      properties.push_back(LiteralPrototype(ctx_, *this, depth));
      has_proto = true;
      continue;
    }
    std::string_view name = PropertyName();
    if (name == "__proto__") {
      if (has_proto) {
        continue;
      }
      has_proto = true;
    }
    if (!IsIdentifierName(name) && r.Coin()) {
      properties.push_back(
          mk::Prop(tree(), Key(), Value(JsType::kUnknown, depth + 1), true));
      continue;
    }
    if (depth < kObjectMethodMaxDepth.AsInt() &&
        r.Chance(kObjectMethodChance())) {
      // Method or accessor.
      LiteralMemberSettings settings;
      settings.depth = depth;
      settings.getter_chance = kObjectGetterChance();
      settings.max_extra_statements = kObjectMethodMaxExtraStatements.AsInt();
      properties.push_back(LiteralMethod(ctx_, *this, name, settings));
      continue;
    }
    if (!ctx_.cannot_throw && r.Chance(kObjectVariableKeyChance())) {
      // Fuzzilli: ObjectLiteralComputedPropertyGenerator, :900.
      if (fast::Identifier* id = Variable()) {
        properties.push_back(
            mk::Prop(tree(), id, Value(JsType::kUnknown, depth + 1), true));
        continue;
      }
    }
    // A spread runs the getters of the source object.
    if (!ctx_.cannot_throw && r.Chance(kObjectSpreadChance())) {
      if (fast::Expression* spread = Variable(JsType::kObject)) {
        properties.push_back(mk::Spread(tree(), spread));
        continue;
      }
    }
    std::string_view key = IsIdentifierName(name) ? name : "p";
    if (r.Chance(kObjectLiteralKeyChance())) {
      // A literal key: `{1: v}`, `{"a b": v}`, `{10n: v}`. These are not
      // computed keys: the parser canonicalizes them while building the
      // literal's boilerplate, a path identifier keys never take.
      fast::Expression* literal_key = nullptr;
      switch (r.Index(3)) {
        case 0: {
          // A numeric key is a plain literal: no sign allowed.
          fast::Expression* integer = Integer();
          if (auto* number = integer->As<NumericLiteral>()) {
            number->value = std::fabs(number->value);
            literal_key = number;
          } else {
            literal_key = mk::Num(tree(), static_cast<double>(r.Index(1000)));
          }
          break;
        }
        case 1:
          literal_key = String();
          if (auto* str = literal_key->As<StringLiteral>();
              str != nullptr && str->value == "__proto__") {
            if (has_proto) {
              continue;
            }
            has_proto = true;
          }
          break;
        default:
          literal_key = BigInt();
          break;
      }
      properties.push_back(mk::Prop(tree(), literal_key,
                                    Value(JsType::kUnknown, depth + 1),
                                    /*computed=*/false));
      continue;
    }
    if (!IsIdentifierName(name) && !name.starts_with("Symbol.")) {
      // A string or numeric key such as "!" or 42. A well-known symbol
      // keeps the plain key: a non-function value there makes coercions
      // throw.
      bool computed = false;
      fast::Expression* name_key = LiteralKey(tree(), name, &computed);
      properties.push_back(mk::Prop(
          tree(), name_key, Value(JsType::kUnknown, depth + 1), computed));
      continue;
    }
    properties.push_back(
        mk::Prop(tree(), key, Value(JsType::kUnknown, depth + 1)));
  }
  return mk::Object(tree(), std::move(properties));
}

REDLINE_TUNABLE(kArrayMaxElements, "codegen.array.max_elements", 8, 0, 64,
                "most elements an array literal gets");
REDLINE_TUNABLE(kArrayElementContinueChance,
                "codegen.array.element_continue_chance", 0.7, 0, 1,
                "probability of adding one more element to an array literal "
                "(geometric count)");
REDLINE_TUNABLE(kArrayHoleChance, "codegen.array.hole_chance", 0.05, 0, 1,
                "probability that an array literal element is a hole");
REDLINE_TUNABLE(kArraySpreadChance, "codegen.array.spread_chance", 0.05, 0, 1,
                "probability that an array literal element is a spread of a "
                "visible array variable");
REDLINE_TUNABLE(kArrayIntegerElementChance,
                "codegen.array.integer_element_chance", 0.7, 0, 1,
                "probability that an element of a numeric array literal is an "
                "integer literal rather than any number value");
REDLINE_TUNABLE(kArrayHoleyNumberChance, "codegen.array.holey_number_chance",
                0.1, 0, 1,
                "probability that an array literal has numbers and at least "
                "one hole ([1, , 3]), for HOLEY_SMI_ELEMENTS and "
                "HOLEY_DOUBLE_ELEMENTS");
REDLINE_TUNABLE(kArrayHoleyNumberHoleChance,
                "codegen.array.holey_number_hole_chance", 0.3, 0, 1,
                "probability that an element of such an array literal is a "
                "hole");

fast::Expression* CodeGen::ArrayLiteral(int depth) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return mk::Array(tree(), {});
  }
  Random& r = rng();
  std::vector<fast::Expression*> elements;
  int n =
      r.SmallCount(kArrayMaxElements.AsInt(), kArrayElementContinueChance());
  if (r.Chance(kArrayHoleyNumberChance())) {
    bool hole = false;
    for (int i = 0; i < n; i++) {
      if (r.Chance(kArrayHoleyNumberHoleChance())) {
        elements.push_back(nullptr);
        hole = true;
      } else if (r.Chance(kArrayIntegerElementChance())) {
        elements.push_back(Integer());
      } else {
        elements.push_back(Value(JsType::kNumber, depth + 1));
      }
    }
    if (!hole) {
      // At least one hole.
      elements.insert(elements.begin() + static_cast<std::ptrdiff_t>(
                                             r.Index(elements.size() + 1)),
                      nullptr);
    }
    return mk::Array(tree(), std::move(elements));
  }
  JsType element_type = JsType::kUnknown;
  // Homogeneous arrays are common and exercise elements kinds.
  switch (r.Index(5)) {
    case 0:
      element_type = JsType::kNumber;
      break;
    case 1:
      element_type = JsType::kString;
      break;
    case 2:
      element_type = JsType::kObject;
      break;
    default:
      break;
  }
  for (int i = 0; i < n; i++) {
    if (r.Chance(kArrayHoleChance())) {
      elements.push_back(nullptr);  // hole
    } else if (!ctx_.cannot_throw && r.Chance(kArraySpreadChance())) {
      // Variable(kArray) can fall back to a value that is not iterable.
      if (fast::Identifier* id = Variable(JsType::kArray)) {
        elements.push_back(mk::Spread(tree(), id));
        continue;
      }
      elements.push_back(Value(element_type, depth + 1));
    } else if (element_type == JsType::kNumber &&
               r.Chance(kArrayIntegerElementChance())) {
      elements.push_back(Integer());
    } else {
      elements.push_back(Value(element_type, depth + 1));
    }
  }
  return mk::Array(tree(), std::move(elements));
}

REDLINE_TUNABLE(kFunctionMaxParams, "codegen.function.max_params", 3, 0, 16,
                "most parameters a generated function expression declares");
REDLINE_TUNABLE(
    kFunctionArrowChance, "codegen.function.arrow_chance", 0.4, 0, 1,
    "probability that a generated function expression is an arrow function");
REDLINE_TUNABLE(kFunctionAsyncChance, "codegen.function.async_chance", 0.1, 0,
                1, "probability that a generated function expression is async");
REDLINE_TUNABLE(kFunctionGeneratorChance, "codegen.function.generator_chance",
                0.08, 0, 1,
                "probability that a generated non-arrow function expression is "
                "a generator");
REDLINE_TUNABLE(kFunctionMaxExtraStatements,
                "codegen.function.max_extra_statements", 3, 0, 64,
                "most statements a generated function body gets beyond its "
                "first one (geometric count)");

REDLINE_TUNABLE(kParametersRestChance, "codegen.parameters.rest_chance", 0.2, 0,
                1,
                "probability that the last parameter of a generated function "
                "is a rest parameter (...a)");
REDLINE_TUNABLE(kParametersDefaultChance, "codegen.parameters.default_chance",
                0.1, 0, 1,
                "probability that a parameter of a generated function gets a "
                "default value (a = v)");

REDLINE_TUNABLE(kParametersTypeThreshold, "codegen.parameters.type_threshold",
                3, 1, 64,
                "fewest visible variables of a type that make the type a "
                "candidate for a typed parameter");
REDLINE_TUNABLE(kParametersUntypedChance, "codegen.parameters.untyped_chance",
                0.2, 0, 1,
                "probability that a parameter of a typed function has no type "
                "even if typed candidates exist");

// Fuzzilli's randomParameters() (ProgramBuilder.swift:641-707). A parameter
// type is a type that at least a few visible variables have. Then the calls
// of the function can pass these variables.
std::vector<JsType> CodeGen::ParameterTypes(int count) {
  Random& r = rng();
  int counts[256] = {};
  for (const redline::Variable& v : ctx_.visible) {
    if (v.name != "arguments") {
      counts[static_cast<uint8_t>(v.type)]++;
    }
  }
  std::vector<JsType> candidates;
  for (int i = 0; i < 256; i++) {
    const JsType type = static_cast<JsType>(i);
    if (type != JsType::kUnknown &&
        counts[i] >= kParametersTypeThreshold.AsInt()) {
      candidates.push_back(type);
    }
  }
  std::vector<JsType> types;
  types.reserve(static_cast<size_t>(std::max(count, 0)));
  for (int i = 0; i < count; i++) {
    if (candidates.empty() || r.Chance(kParametersUntypedChance())) {
      types.push_back(JsType::kUnknown);
    } else {
      types.push_back(r.Pick(candidates));
    }
  }
  return types;
}

std::vector<fast::Expression*> CodeGen::Parameters(
    GenContext& nested, int count, std::string_view prefix,
    std::vector<std::string_view>* names, std::vector<JsType>* types) {
  Random& r = rng();
  std::vector<fast::Expression*> params;
  const bool rest = count > 0 && r.Chance(kParametersRestChance());
  std::vector<JsType> param_types;
  if (types != nullptr) {
    param_types = ParameterTypes(count);
    *types = param_types;
  } else {
    param_types.assign(static_cast<size_t>(std::max(count, 0)),
                       JsType::kUnknown);
  }
  // A default value sees the enclosing variables and the earlier parameters.
  CodeGen defaults(nested);
  const size_t before = nested.visible.size();
  for (int i = 0; i < count; i++) {
    std::string_view name = ctx_.builder.FreshName(prefix);
    if (names != nullptr) {
      names->push_back(name);
    }
    if (rest && i == count - 1) {
      params.push_back(mk::Rest(tree(), mk::Id(tree(), name)));
      nested.Declare(name, JsType::kArray);
      continue;
    }
    const JsType type = param_types[static_cast<size_t>(i)];
    fast::Expression* param = mk::Id(tree(), name);
    if (r.Chance(kParametersDefaultChance())) {
      param = mk::WithDefault(tree(), param, defaults.Value(type, 1));
    }
    params.push_back(param);
    nested.Declare(name, type);
  }
  // Declare() inserts at the front, so the parameters are now last first.
  // Put the first parameter first: JitOptimize gives its warm-up values to
  // the first argument.
  const size_t added = nested.visible.size() - before;
  if (added > 1) {
    std::reverse(nested.visible.begin(),
                 nested.visible.begin() + static_cast<std::ptrdiff_t>(added));
  }
  return params;
}

// static
bool CodeGen::SimpleParameters(const std::vector<fast::Expression*>& params) {
  for (const fast::Expression* param : params) {
    if (!param->Is<Identifier>()) {
      return false;
    }
  }
  return true;
}

fast::Statement* CodeGen::UseStrict() {
  ExpressionStatement* s = mk::ExprStmt(tree(), mk::Str(tree(), "use strict"));
  s->is_directive = true;
  s->directive = "use strict";
  return s;
}

REDLINE_TUNABLE(kFunctionStrictChance, "codegen.function.strict_chance", 0.1, 0,
                1,
                "probability that a generated function body with simple "
                "parameters starts with a 'use strict' directive");

fast::Expression* CodeGen::FunctionValue(int depth,
                                         std::vector<JsType>* parameter_types) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    if (parameter_types != nullptr) {
      parameter_types->clear();
    }
    return mk::ArrowExpr(tree(), {}, Number());
  }
  Random& r = rng();
  int n = static_cast<int>(r.Range(0, kFunctionMaxParams.AsInt()));
  GenContext nested = ctx_.Nested(true, /*is_arrow=*/false, false, false);
  bool arrow = r.Chance(kFunctionArrowChance());
  bool is_async = r.Chance(kFunctionAsyncChance());
  bool is_generator = !arrow && r.Chance(kFunctionGeneratorChance());
  nested.syntax.in_async = is_async;
  nested.syntax.in_generator = is_generator;
  if (arrow) {
    // Arrows keep the enclosing this/arguments/super context.
    nested.syntax.has_arguments = ctx_.syntax.has_arguments;
    nested.syntax.in_method = ctx_.syntax.in_method;
    nested.syntax.in_constructor = false;
    nested.syntax.in_class_field = ctx_.syntax.in_class_field;
    nested.syntax.in_static_block = ctx_.syntax.in_static_block;
  }
  // After the context is complete: default values must respect it.
  std::vector<fast::Expression*> params =
      Parameters(nested, n, "a", nullptr, parameter_types);
  // A "use strict" directive (Fuzzilli's StrictModeFunctionGenerator,
  // CodeGenerators.swift:1703) needs a simple parameter list.
  const bool strict = !ctx_.syntax.strict && SimpleParameters(params) &&
                      r.Chance(kFunctionStrictChance());
  if (strict) {
    nested.syntax.strict = true;
  }
  CodeGen inner(nested);
  std::vector<Statement*> body;
  if (depth < ctx_.max_depth) {
    body = inner.Body(nested,
                      1 + r.SmallCount(kFunctionMaxExtraStatements.AsInt()));
  }
  if (arrow && body.empty() && !strict && r.Coin()) {
    ArrowFunctionExpression* fn =
        mk::ArrowExpr(tree(), {}, inner.Expression(depth + 1), is_async);
    fn->params = std::move(params);
    return fn;
  }
  body.push_back(mk::Return(tree(), inner.Value(JsType::kUnknown, depth + 1)));
  if (strict) {
    body.insert(body.begin(), UseStrict());
  }
  if (arrow) {
    ArrowFunctionExpression* fn =
        mk::Arrow(tree(), {}, std::move(body), is_async);
    fn->params = std::move(params);
    fn->strict = strict;
    return fn;
  }
  FunctionExpression* fn =
      mk::Function(tree(), "", {}, std::move(body), is_async, is_generator);
  fn->params = std::move(params);
  fn->strict = strict;
  return fn;
}

REDLINE_TUNABLE(kCallableVariableChance, "codegen.callable.variable_chance",
                0.7, 0, 1,
                "probability that a callee is a visible function variable");
REDLINE_TUNABLE(
    kCallableBuiltinChance, "codegen.callable.builtin_chance", 0.5, 0, 1,
    "probability that a callee is a builtin function-valued global");
REDLINE_TUNABLE(kCallableClassChance, "codegen.callable.class_chance", 0.5, 0,
                1, "probability that a callee is a visible class variable");

fast::Expression* CodeGen::Callable() {
  Random& r = rng();
  if (r.Chance(kCallableVariableChance())) {
    if (fast::Identifier* id = Variable(JsType::kFunction)) {
      return id;
    }
  }
  if (r.Chance(kCallableBuiltinChance())) {
    // A builtin function-valued global.
    std::vector<const BuiltinGlobal*> functions;
    for (const BuiltinGlobal& g : env().globals()) {
      if (g.type == JsType::kFunction && !g.constructible) {
        functions.push_back(&g);
      }
    }
    if (!functions.empty()) {
      return mk::Id(tree(), r.Pick(functions)->name);
    }
  }
  if (r.Chance(kCallableClassChance())) {
    if (fast::Identifier* id = Variable(JsType::kClass)) {
      return id;
    }
  }
  return FunctionValue(ctx_.depth + 1);
}

REDLINE_TUNABLE(kObjectLikeVariableChance,
                "codegen.object_like.variable_chance", 0.75, 0, 1,
                "probability that an object-like operand is a visible variable "
                "rather than a builtin or a fresh literal");
REDLINE_TUNABLE(kObjectLikeBuiltinChance, "codegen.object_like.builtin_chance",
                0.15, 0, 1,
                "probability that an object-like operand is a builtin "
                "namespace or constructor object");

fast::Expression* CodeGen::ObjectLike() {
  Random& r = rng();
  static const JsType kObjectTypes[] = {
      JsType::kObject, JsType::kArray,      JsType::kFunction, JsType::kMap,
      JsType::kSet,    JsType::kTypedArray, JsType::kClass,    JsType::kRegExp,
      JsType::kDate,   JsType::kError,      JsType::kPromise,  JsType::kString};
  if (r.Chance(kObjectLikeVariableChance())) {
    JsType type =
        kObjectTypes[r.Index(sizeof(kObjectTypes) / sizeof(kObjectTypes[0]))];
    if (fast::Identifier* id = Variable(type)) {
      return id;
    }
    if (fast::Identifier* id = Variable(JsType::kObject)) {
      return id;
    }
    if (r.Coin()) {
      if (fast::Identifier* id = Variable(JsType::kUnknown)) {
        return id;
      }
    }
  }
  if (r.Chance(kObjectLikeBuiltinChance())) {
    // A builtin namespace or constructor object.
    const BuiltinGlobal& g = r.Pick(env().globals());
    if (g.type == JsType::kObject || g.constructible) {
      return GlobalPath(g.name);
    }
  }
  if (r.Coin()) {
    return ObjectLiteral(1);
  }
  return ArrayLiteral(1);
}

REDLINE_TUNABLE(kTargetVariableChance, "codegen.target.variable_chance", 0.5, 0,
                1,
                "probability that an assignment target is a mutable visible "
                "variable rather than a property");
REDLINE_TUNABLE(kPropertyNameMemberPoolChance,
                "codegen.property_name.member_pool_chance", 0.25, 0, 1,
                "probability that a property name comes from the small pool "
                "of member names that classes and object literals use");

fast::Expression* CodeGen::Target() {
  Random& r = rng();
  if (r.Chance(kTargetVariableChance())) {
    if (fast::Identifier* id = MutableVariable()) {
      return id;
    }
  }
  fast::Expression* object = ObjectLike();
  if (object->Is<ObjectExpression>() || object->Is<ArrayExpression>()) {
    // `[1,2].x = 3` is legal but pointless; prefer a variable if any.
    if (fast::Identifier* id = MutableVariable()) {
      return id;
    }
  }
  return Property(object);
}

std::string_view CodeGen::PropertyName() {
  if (rng().Chance(kPropertyNameMemberPoolChance())) {
    return RandomMemberPropertyName(rng());
  }
  return rng().Pick(env().property_names());
}

fast::Expression* CodeGen::Property(fast::Expression* object,
                                    std::string_view name) {
  if (IsIdentifierName(name)) {
    return mk::Member(tree(), object, name);
  }
  if (name.starts_with("Symbol.")) {
    return mk::Index(
        tree(), object,
        mk::Member(tree(), mk::Id(tree(), "Symbol"), name.substr(7)));
  }
  // Numeric or otherwise non-identifier key.
  bool numeric = !name.empty();
  for (char c : name) {
    if (!((c >= '0' && c <= '9') || c == '.' || c == '-')) {
      numeric = false;
    }
  }
  if (numeric) {
    double value = std::strtod(std::string(name).c_str(), nullptr);
    return mk::Index(tree(), object, mk::Num(tree(), value));
  }
  return mk::Index(tree(), object, mk::Str(tree(), name));
}

REDLINE_TUNABLE(kPropertyComputedChance, "codegen.property.computed_chance",
                0.15, 0, 1,
                "probability that a property access is computed with a "
                "variable or expression key");
REDLINE_TUNABLE(kPropertyTypedNameChance, "codegen.property.typed_name_chance",
                0.6, 0, 1,
                "probability that a property access on a value of known type "
                "uses one of that type's properties");
REDLINE_TUNABLE(kPropertyKnownKeyChance, "codegen.property.known_key_chance",
                0.5, 0, 1,
                "probability that a property access on a variable with known "
                "keys (literal keys, class members, earlier stores) uses one");

fast::Expression* CodeGen::Property(fast::Expression* object) {
  Random& r = rng();
  // A key that the program gave the value: the access finds a property.
  // Fuzzilli: PropertyRetrievalGenerator, CodeGen/CodeGenerators.swift:1932.
  if (r.Chance(kPropertyKnownKeyChance())) {
    if (std::optional<KnownKey> key = RandomKnownKey(ctx_, object, false)) {
      return Property(object, key->name);
    }
  }
  if (r.Chance(kPropertyComputedChance())) {
    // Computed access with a variable or expression key.
    if (fast::Identifier* key =
            Variable(r.Coin() ? JsType::kNumber : JsType::kString)) {
      return mk::Index(tree(), object, key);
    }
    return mk::Index(tree(), object, Key());
  }
  // Type-directed property names when we know the type.
  JsType type = Builder::GuessType(object);
  if (object->Is<Identifier>()) {
    for (const redline::Variable& v : ctx_.visible) {
      if (v.name == object->Cast<Identifier>()->name) {
        type = v.type;
        break;
      }
    }
  }
  if (const BuiltinType* bt = env().ForType(type)) {
    if (!bt->properties.empty() && r.Chance(kPropertyTypedNameChance())) {
      return Property(object, r.Pick(bt->properties));
    }
  }
  return Property(object, PropertyName());
}

fast::Expression* CodeGen::Key() {
  Random& r = rng();
  switch (r.Index(6)) {
    case 0:
    case 1:
      return Integer();
    case 2:
      return mk::Str(tree(), PropertyName());
    case 3:
      return String();
    case 4:
      return mk::Member(
          tree(), mk::Id(tree(), "Symbol"),
          r.Pick({"iterator", "toPrimitive", "hasInstance", "species",
                  "toStringTag", "asyncIterator", "isConcatSpreadable"}));
    default:
      return Number();
  }
}

REDLINE_TUNABLE(kArgumentsArityViolationChance,
                "codegen.arguments.arity_violation_chance", 0.05, 0, 1,
                "probability that a builtin call ignores the builtin's arity "
                "and passes a random number of arguments");
REDLINE_TUNABLE(kArgumentsArityViolationMaxArgs,
                "codegen.arguments.arity_violation_max_args", 4, 0, 16,
                "most arguments an arity-violating builtin call passes");
REDLINE_TUNABLE(kArgumentsCallbackArrowChance,
                "codegen.arguments.callback_arrow_chance", 0.8, 0, 1,
                "probability that a function-typed argument is a fresh short "
                "arrow rather than an existing function value");
REDLINE_TUNABLE(kArgumentsCallbackMaxParams,
                "codegen.arguments.callback_max_params", 2, 0, 16,
                "most parameters a callback arrow argument declares");
REDLINE_TUNABLE(kArgumentsCallbackThisChance,
                "codegen.arguments.callback_this_chance", 0.15, 0, 1,
                "probability that a function-typed argument is a new function "
                "expression that reads a property of this (the thisArg of "
                "forEach, map and the other iterating methods binds it)");

std::vector<fast::Expression*> CodeGen::Arguments(
    const BuiltinFunction& function) {
  Random& r = rng();
  if (!function.params.empty() && !r.Chance(kArgumentsArityViolationChance())) {
    // The model knows every parameter (codegen_typed.cc).
    return ArgumentsFor(function.params);
  }
  int n = static_cast<int>(r.Range(function.min_args, function.max_args));
  if (r.Chance(kArgumentsArityViolationChance())) {
    n = static_cast<int>(r.Range(
        0, kArgumentsArityViolationMaxArgs.AsInt()));  // arity violations
  }
  std::vector<fast::Expression*> args;
  for (int i = 0; i < n; i++) {
    JsType hint = i < 3 ? function.args[i] : JsType::kUnknown;
    if (hint == JsType::kFunction && r.Chance(kArgumentsCallbackThisChance())) {
      // A function, not an arrow: its `this` is the receiver of the call.
      int np =
          static_cast<int>(r.Range(0, kArgumentsCallbackMaxParams.AsInt()));
      GenContext nested = ctx_.Nested(true, false, false, false);
      std::vector<fast::Expression*> params = Parameters(nested, np, "e");
      CodeGen inner(nested);
      FunctionExpression* callback =
          mk::Function(tree(), "", {},
                       {mk::Return(tree(), inner.Property(mk::This(tree())))});
      callback->params = std::move(params);
      args.push_back(callback);
      continue;
    }
    if (hint == JsType::kFunction &&
        r.Chance(kArgumentsCallbackArrowChance())) {
      // Callbacks: prefer short arrows over existing functions.
      int np =
          static_cast<int>(r.Range(0, kArgumentsCallbackMaxParams.AsInt()));
      GenContext nested = ctx_.Nested(true, true, false, false);
      std::vector<fast::Expression*> params = Parameters(nested, np, "e");
      CodeGen inner(nested);
      ArrowFunctionExpression* callback =
          mk::ArrowExpr(tree(), {}, inner.Expression(2));
      callback->params = std::move(params);
      args.push_back(callback);
      continue;
    }
    args.push_back(Value(hint, 1));
  }
  return args;
}

std::vector<fast::Expression*> CodeGen::Arguments(int min_args, int max_args) {
  int n = static_cast<int>(rng().Range(min_args, max_args));
  std::vector<fast::Expression*> args;
  if (n > 0) {
    args.reserve(static_cast<size_t>(n));
  }
  for (int i = 0; i < n; i++) {
    args.push_back(Value(JsType::kUnknown, 1));
  }
  return args;
}

fast::Expression* CodeGen::BuiltinCall() {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return Leaf(JsType::kUnknown);
  }
  Random& r = rng();
  std::vector<const BuiltinGlobal*> candidates;
  for (const BuiltinGlobal& g : env().globals()) {
    if (!g.static_methods.empty() ||
        (g.type == JsType::kFunction && !g.constructible)) {
      candidates.push_back(&g);
    }
  }
  if (candidates.empty()) {
    return Literal();
  }
  const BuiltinGlobal& g = *r.Pick(candidates);
  if (g.static_methods.empty() ||
      (g.type == JsType::kFunction && !g.constructible && r.Coin())) {
    BuiltinFunction f{g.name, g.min_args, g.max_args, g.instance_type};
    f.params = g.params;
    return mk::Call(tree(), GlobalPath(g.name), Arguments(f));
  }
  const BuiltinFunction& m = r.Pick(g.static_methods);
  return CallMethod(GlobalPath(g.name), m.name, Arguments(m));
}

REDLINE_TUNABLE(kMethodCallGenericChance, "codegen.method_call.generic_chance",
                0.1, 0, 1,
                "probability that a method call on a value of known type uses "
                "a generic Object method instead of one of the type's "
                "methods");
REDLINE_TUNABLE(kMethodCallComputedChance,
                "codegen.method_call.computed_chance", 0.1, 0, 1,
                "probability that a method call reads the method with a "
                "computed key (o[\"m\"](...)) rather than with a dot");
REDLINE_TUNABLE(kMethodCallSpreadChance, "codegen.method_call.spread_chance",
                0.1, 0, 1,
                "probability that a method call appends a spread of an array "
                "to its arguments");

fast::CallExpression* CodeGen::CallMethod(fast::Expression* object,
                                          std::string_view name,
                                          std::vector<fast::Expression*> args) {
  Random& r = rng();
  if (r.Chance(kMethodCallSpreadChance())) {
    // Any iterable: a value of another type can throw.
    args.push_back(mk::Spread(tree(), AnyIterable(*this)));
  }
  fast::Expression* callee =
      r.Chance(kMethodCallComputedChance())
          ? static_cast<fast::Expression*>(
                mk::Index(tree(), object, mk::Str(tree(), name)))
          : static_cast<fast::Expression*>(mk::Member(tree(), object, name));
  return mk::Call(tree(), callee, std::move(args));
}

REDLINE_TUNABLE(kMethodCallKnownMethodChance,
                "codegen.method_call.known_method_chance", 0.5, 0, 1,
                "probability that a method call on a variable with known "
                "methods (class or literal methods) calls one of them");

fast::Expression* CodeGen::MethodCall(fast::Expression* object, JsType type) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return mk::CallMethod(tree(), object, "valueOf", {});
  }
  Random& r = rng();
  // A method that the program gave the value, with one argument for each of
  // its parameters. Fuzzilli: MethodCallGenerator, CodeGenerators.swift:2262.
  if (r.Chance(kMethodCallKnownMethodChance())) {
    if (std::optional<KnownKey> method = RandomKnownKey(ctx_, object, true)) {
      std::vector<fast::Expression*> args;
      for (int i = 0; i < method->arity; i++) {
        args.push_back(Value(JsType::kUnknown, 1));
      }
      return mk::Call(tree(), Property(object, method->name), std::move(args));
    }
  }
  if (type == JsType::kUnknown && object->Is<Identifier>()) {
    for (const redline::Variable& v : ctx_.visible) {
      if (v.name == object->Cast<Identifier>()->name) {
        type = v.type;
        break;
      }
    }
  }
  const BuiltinType* bt = env().ForType(type);
  if (bt == nullptr || bt->methods.empty() ||
      r.Chance(kMethodCallGenericChance())) {
    // Generic object methods, or a guess.
    static const BuiltinFunction kGeneric[] = {
        {"toString", 0, 0, JsType::kString},
        {"valueOf", 0, 0, JsType::kUnknown},
        {"hasOwnProperty", 1, 1, JsType::kBoolean, {JsType::kString}},
        {"isPrototypeOf", 1, 1, JsType::kBoolean, {JsType::kObject}},
        {"propertyIsEnumerable", 1, 1, JsType::kBoolean, {JsType::kString}},
        {"toLocaleString", 0, 0, JsType::kString},
    };
    const BuiltinFunction& m =
        kGeneric[r.Index(sizeof(kGeneric) / sizeof(kGeneric[0]))];
    return CallMethod(object, m.name, Arguments(m));
  }
  const BuiltinFunction& m = r.Pick(bt->methods);
  return CallMethod(object, m.name, Arguments(m));
}

REDLINE_TUNABLE(kConstructMaxUserArgs, "codegen.construct.max_user_args", 2, 0,
                16,
                "most arguments passed when constructing a visible "
                "user-defined class or function");
REDLINE_TUNABLE(kConstructTypedArraySizeChance,
                "codegen.construct.typed_array_size_chance", 0.7, 0, 1,
                "probability that a typed array or ArrayBuffer is constructed "
                "from a size rather than from a buffer or array");

fast::Expression* CodeGen::Construct(JsType type) {
  ExprDepthGuard guard(ctx_);
  if (TooDeep()) {
    return type == JsType::kUnknown
               ? mk::New(tree(), mk::Id(tree(), "Object"), {})
               : nullptr;
  }
  Random& r = rng();
  std::vector<const BuiltinGlobal*> candidates;
  if (type == JsType::kUnknown) {
    for (const BuiltinGlobal& g : env().globals()) {
      if (g.constructible) {
        candidates.push_back(&g);
      }
    }
  } else {
    candidates = env().ConstructorsOf(type);
  }
  if (candidates.empty()) {
    if (type == JsType::kUnknown || type == JsType::kObject ||
        type == JsType::kClass) {
      if (fast::Identifier* id = Variable(JsType::kClass)) {
        return mk::New(tree(), id, Arguments(0, kConstructMaxUserArgs.AsInt()));
      }
      if (fast::Identifier* id = Variable(JsType::kFunction)) {
        return mk::New(tree(), id, Arguments(0, kConstructMaxUserArgs.AsInt()));
      }
    }
    return nullptr;
  }
  const BuiltinGlobal& g = *r.Pick(candidates);
  BuiltinFunction f{g.name, g.min_args, g.max_args, g.instance_type};
  f.params = g.params;
  // Typed arrays and ArrayBuffers take sizes; promises take executors.
  if (g.instance_type == JsType::kTypedArray ||
      g.instance_type == JsType::kArrayBuffer) {
    if (r.Chance(kConstructTypedArraySizeChance())) {
      return mk::New(tree(), mk::Id(tree(), g.name), {Integer()});
    }
    if (fast::Identifier* id = Variable(JsType::kArrayBuffer)) {
      return mk::New(tree(), mk::Id(tree(), g.name), {id});
    }
    return mk::New(tree(), mk::Id(tree(), g.name), {ArrayLiteral(1)});
  }
  if (g.instance_type == JsType::kPromise) {
    std::vector<std::string_view> params = {ctx_.builder.FreshName("resolve"),
                                            ctx_.builder.FreshName("reject")};
    std::vector<Statement*> body = {
        Stmt(mk::Call(tree(), mk::Id(tree(), params[r.Index(2)]),
                      {Value(JsType::kUnknown, 1)}))};
    return mk::New(tree(), mk::Id(tree(), g.name),
                   {mk::Arrow(tree(), params, std::move(body))});
  }
  if (g.instance_type == JsType::kFunction && g.name == "Function") {
    return mk::New(tree(), mk::Id(tree(), g.name),
                   {mk::Str(tree(), "a"), mk::Str(tree(), "return a")});
  }
  if (g.name == "Proxy") {
    return mk::New(tree(), mk::Id(tree(), g.name),
                   {ObjectLike(), ObjectLiteral(1)});
  }
  return mk::New(tree(), GlobalPath(g.name), Arguments(f));
}

// --- operators
// -------------------------------------------------------------------

// static
BinaryOp CodeGen::RandomArithmeticOp(Random& r) {
  static const BinaryOp kOps[] = {
      BinaryOp::kAdd,        BinaryOp::kSub,
      BinaryOp::kMul,        BinaryOp::kDiv,
      BinaryOp::kMod,        BinaryOp::kExp,
      BinaryOp::kBitOr,      BinaryOp::kBitXor,
      BinaryOp::kBitAnd,     BinaryOp::kShiftLeft,
      BinaryOp::kShiftRight, BinaryOp::kShiftRightLogical};
  return kOps[r.Index(sizeof(kOps) / sizeof(kOps[0]))];
}

// static
BinaryOp CodeGen::RandomComparisonOp(Random& r) {
  static const BinaryOp kOps[] = {
      BinaryOp::kEq,          BinaryOp::kNotEq,        BinaryOp::kStrictEq,
      BinaryOp::kStrictNotEq, BinaryOp::kLessThan,     BinaryOp::kLessThanEq,
      BinaryOp::kGreaterThan, BinaryOp::kGreaterThanEq};
  return kOps[r.Index(sizeof(kOps) / sizeof(kOps[0]))];
}

REDLINE_TUNABLE(kBinaryArithmeticOpChance,
                "codegen.binary.arithmetic_op_chance", 0.7, 0, 1,
                "probability that a random binary operator is arithmetic "
                "rather than a comparison");

// static
BinaryOp CodeGen::RandomBinaryOp(Random& r) {
  return r.Chance(kBinaryArithmeticOpChance()) ? RandomArithmeticOp(r)
                                               : RandomComparisonOp(r);
}

// static
UnaryOp CodeGen::RandomUnaryOp(Random& r) {
  static const UnaryOp kOps[] = {UnaryOp::kMinus,  UnaryOp::kPlus,
                                 UnaryOp::kNot,    UnaryOp::kBitNot,
                                 UnaryOp::kTypeof, UnaryOp::kVoid};
  return kOps[r.Index(sizeof(kOps) / sizeof(kOps[0]))];
}

// static
AssignOp CodeGen::RandomCompoundAssignOp(Random& r) {
  static const AssignOp kOps[] = {AssignOp::kAddAssign,
                                  AssignOp::kSubAssign,
                                  AssignOp::kMulAssign,
                                  AssignOp::kDivAssign,
                                  AssignOp::kModAssign,
                                  AssignOp::kExpAssign,
                                  AssignOp::kShiftLeftAssign,
                                  AssignOp::kShiftRightAssign,
                                  AssignOp::kShiftRightLogicalAssign,
                                  AssignOp::kBitOrAssign,
                                  AssignOp::kBitXorAssign,
                                  AssignOp::kBitAndAssign,
                                  AssignOp::kOrAssign,
                                  AssignOp::kAndAssign,
                                  AssignOp::kNullishAssign};
  return kOps[r.Index(sizeof(kOps) / sizeof(kOps[0]))];
}

// static
LogicalOp CodeGen::RandomLogicalOp(Random& r) {
  return r.Pick({LogicalOp::kAnd, LogicalOp::kOr, LogicalOp::kNullish});
}

REDLINE_TUNABLE(kBinaryBigintChance, "codegen.binary.bigint_chance", 0.1, 0, 1,
                "probability that a binary arithmetic operation is on BigInts");
REDLINE_TUNABLE(kBinaryMixedTypeChance, "codegen.binary.mixed_type_chance",
                0.15, 0, 1,
                "probability that a binary arithmetic operation takes operands "
                "of arbitrary type");

fast::Expression* CodeGen::BinaryOperation(int depth) {
  Random& r = rng();
  BinaryOp op = RandomArithmeticOp(r);
  JsType type =
      r.Chance(kBinaryBigintChance()) ? JsType::kBigInt : JsType::kNumber;
  if (r.Chance(kBinaryMixedTypeChance())) {
    type = JsType::kUnknown;  // mixed-type arithmetic
  }
  fast::Expression* left = Value(type, depth);
  fast::Expression* right = Value(type, depth);
  // `-a ** b` is a syntax error; the printer parenthesizes unary operands,
  // but avoid the pattern anyway.
  if (op == BinaryOp::kExp && left->Is<UnaryExpression>()) {
    op = BinaryOp::kMul;
  }
  return mk::Binary(tree(), op, left, right);
}

fast::Expression* CodeGen::Comparison(int depth) {
  Random& r = rng();
  JsType type = r.Coin() ? JsType::kNumber : JsType::kUnknown;
  fast::Expression* left = Value(type, depth);
  fast::Expression* right = Value(type, depth);
  return mk::Binary(tree(), RandomComparisonOp(r), left, right);
}

REDLINE_TUNABLE(kUnaryDeleteChance, "codegen.unary.delete_chance", 0.1, 0, 1,
                "probability that a unary operation is a property delete");
REDLINE_TUNABLE(kUnaryUpdateChance, "codegen.unary.update_chance", 0.3, 0, 1,
                "probability that a unary operation is ++ or -- on a mutable "
                "number variable");

fast::Expression* CodeGen::UnaryOperation(int depth) {
  Random& r = rng();
  UnaryOp op = RandomUnaryOp(r);
  if (r.Chance(kUnaryDeleteChance())) {
    // delete obj.prop
    return mk::Unary(tree(), UnaryOp::kDelete, Property(ObjectLike()));
  }
  if (r.Chance(kUnaryUpdateChance())) {
    if (fast::Identifier* id = MutableVariable(JsType::kNumber)) {
      return mk::Update(tree(),
                        r.Coin() ? UpdateOp::kIncrement : UpdateOp::kDecrement,
                        r.Coin(), id);
    }
  }
  JsType type = op == UnaryOp::kNot ? JsType::kUnknown : JsType::kNumber;
  return mk::Unary(tree(), op, Value(type, depth));
}

fast::Expression* CodeGen::BooleanMethodCall() {
  struct Candidate {
    std::string_view variable;
    const BuiltinFunction* method;
  };
  std::vector<Candidate> candidates;
  for (const redline::Variable& v : ctx_.visible) {
    const BuiltinType* bt = env().ForType(v.type);
    if (bt == nullptr || v.name == "arguments") {
      continue;
    }
    for (const BuiltinFunction& m : bt->methods) {
      if (m.result == JsType::kBoolean) {
        candidates.push_back({v.name, &m});
      }
    }
  }
  if (candidates.empty()) {
    return nullptr;
  }
  const Candidate& c = rng().Pick(candidates);
  return mk::CallMethod(tree(), mk::Id(tree(), c.variable), c.method->name,
                        Arguments(*c.method));
}

REDLINE_TUNABLE(kConditionMethodCallChance,
                "codegen.condition.method_call_chance", 0.15, 0, 1,
                "probability that a condition is a call of a method that "
                "returns a boolean (s.startsWith(x), re.test(s), m.has(k))");

fast::Expression* CodeGen::Condition(int depth) {
  Random& r = rng();
  if (r.Chance(kConditionMethodCallChance())) {
    if (fast::Expression* call = BooleanMethodCall()) {
      return call;
    }
  }
  switch (r.Index(6)) {
    case 0:
    case 1:
      return Comparison(depth);
    case 2:
      if (fast::Identifier* id = Variable(JsType::kBoolean)) {
        return id;
      }
      return Comparison(depth);
    case 3:
      return mk::Unary(tree(), UnaryOp::kNot, Value(JsType::kUnknown, depth));
    case 4: {
      fast::Expression* object = ObjectLike();
      return mk::Binary(tree(), BinaryOp::kIn, mk::Str(tree(), PropertyName()),
                        object);
    }
    default:
      return Value(JsType::kUnknown, depth);
  }
}

// --- statements
// ------------------------------------------------------------------

std::vector<fast::Statement*> CodeGen::DeclareGuarded(fast::Expression* init,
                                                      JsType type,
                                                      std::string_view prefix) {
  std::string_view name = ctx_.builder.FreshName(prefix);
  std::vector<fast::Statement*> out;
  out.push_back(mk::Let(tree(), name, nullptr));
  out.push_back(mk::Try(
      tree(),
      {mk::ExprStmt(tree(), mk::Assign(tree(), mk::Id(tree(), name), init))},
      "e", {}));
  ctx_.Declare(name, type);
  RecordDeclaredKeys(ctx_, name, init);
  return out;
}

void CodeGen::DeclareMaybeGuarded(std::vector<fast::Statement*>* out,
                                  fast::Expression* init, JsType type,
                                  std::string_view prefix,
                                  double guard_probability) {
  if (rng().Chance(guard_probability)) {
    for (fast::Statement* s : DeclareGuarded(init, type, prefix)) {
      out->push_back(s);
    }
  } else {
    out->push_back(Declare(init, type, prefix));
  }
}

REDLINE_TUNABLE(kDeclareSharedNameChance, "codegen.declare.shared_name_chance",
                0.1, 0, 1,
                "probability that a let or const declaration takes a name of a "
                "small shared pool (shadowing) if that is valid at its "
                "position");
REDLINE_TUNABLE(kDeclareImplicitGlobalChance,
                "codegen.declare.implicit_global_chance", 0.4, 0, 1,
                "probability that a var statement with a shared name in sloppy "
                "code is an assignment to an undeclared name (a new global)");

fast::Statement* CodeGen::Declare(fast::Expression* init, JsType type,
                                  std::string_view prefix) {
  Random& r = rng();
  std::string_view name = ctx_.builder.FreshName(prefix);
  VariableKind kind = VariableKind::kLet;
  switch (r.Index(10)) {
    case 0:
    case 1:
      kind = VariableKind::kConst;
      break;
    case 2:
      kind = VariableKind::kVar;
      break;
    default:
      break;
  }
  // Shadowing with a shared name. Only lexical declarations: callers cast
  // the result to a VariableDeclaration, and some turn `const` into `let`.
  if (kind != VariableKind::kVar && r.Chance(kDeclareSharedNameChance())) {
    if (std::string_view shared = SharedName(kind, init); !shared.empty()) {
      name = shared;
    }
  }
  ctx_.Declare(name, type, kind == VariableKind::kConst);
  RecordDeclaredKeys(ctx_, name, init);
  return mk::Declare(tree(), kind, name, init);
}

fast::Statement* CodeGen::SharedNameStatement(fast::Expression* init,
                                              JsType type) {
  Random& r = rng();
  VariableKind kind = VariableKind::kVar;
  switch (r.Index(4)) {
    case 0:
      kind = VariableKind::kLet;
      break;
    case 1:
      kind = VariableKind::kConst;
      break;
    default:
      break;
  }
  if (ctx_.depth > 0 && kind == VariableKind::kVar) {
    kind = r.Coin() ? VariableKind::kLet : VariableKind::kConst;
  }
  std::string_view name = SharedName(kind, init);
  if (name.empty()) {
    return nullptr;
  }
  if (kind == VariableKind::kVar && !ctx_.syntax.strict &&
      r.Chance(kDeclareImplicitGlobalChance()) && ImplicitGlobalAllowed(name)) {
    // `x = init;` in sloppy code: x becomes a property of the global object.
    ctx_.Declare(name, type);
    return mk::ExprStmt(tree(), mk::Assign(tree(), mk::Id(tree(), name), init));
  }
  ctx_.Declare(name, type, kind == VariableKind::kConst);
  return mk::Declare(tree(), kind, name, init);
}

namespace {

// The shared names of Declare(), after Fuzzilli's small pool of custom
// identifier names (NamedVariableGenerator, CodeGenerators.swift:1650).
const char* const kSharedNames[] = {"a", "b", "c", "d", "e", "f"};

// True if `node` contains an identifier named `name`.
bool Mentions(const Node* node, std::string_view name) {
  bool found = false;
  Walk(const_cast<Node*>(node), [&](Node* n) {
    if (auto* id = n->As<Identifier>(); id != nullptr && id->name == name) {
      found = true;
    }
    return !found;
  });
  return found;
}

}  // namespace

bool CodeGen::NameIsFreeHere(std::string_view name, bool hoists) {
  if (ctx_.body_used || (hoists && ctx_.depth > 0)) {
    return false;
  }
  // A variable of this mutation: it can be in the same block. In a nested
  // block, an outer variable can be used before or after Body().
  for (const redline::Variable& v : ctx_.visible) {
    if (v.name == name && (ctx_.depth > 0 || v.binding == nullptr)) {
      return false;
    }
  }
  // Earlier statements of this block in this mutation.
  if (ctx_.out != nullptr) {
    for (const Statement* s : *ctx_.out) {
      if (Mentions(s, name)) {
        return false;
      }
    }
  }
  if (!ctx_.slot.valid() || (ctx_.depth > 0 && !hoists)) {
    // A block that this mutation makes: the tree has nothing in it.
    return true;
  }
  // The statements of the tree block, before and after the slot: a new
  // binding would capture their references.
  for (const Statement* s : *ctx_.slot.list) {
    if (Mentions(s, name)) {
      return false;
    }
  }
  return true;
}

std::string_view CodeGen::SharedName(VariableKind kind,
                                     const fast::Expression* init) {
  std::string_view name =
      kSharedNames[rng().Index(sizeof(kSharedNames) / sizeof(kSharedNames[0]))];
  const bool hoists = kind == VariableKind::kVar;
  // The case clauses of a switch share one scope. But each clause has a
  // context of its own, so a sibling clause can declare the name too.
  if (ctx_.syntax.in_switch || Mentions(init, name) ||
      !NameIsFreeHere(name, hoists)) {
    return {};
  }
  if (!ctx_.slot.valid()) {
    return name;
  }
  // The tree scopes from the slot. `let x` and `const x` are SyntaxErrors
  // if the scope of the slot declares x (a parameter too). `var x` is a
  // SyntaxError if a block between the slot and the var scope declares x.
  // It is also a SyntaxError if the var scope has a lexical binding x.
  fast::ScopeAnalysis& analysis = ctx_.builder.scopes();
  const fast::Scope* here = analysis.ScopeOf(ctx_.slot.owner);
  if (!hoists && ctx_.depth == 0 && here != nullptr &&
      here->parent != nullptr &&
      (here->kind == fast::ScopeKind::kFunctionBody ||
       here->parent->kind == fast::ScopeKind::kCatch) &&
      here->parent->by_name.contains(name)) {
    // `function f(x = 1) { let x; }` and `catch (x) { let x; }`.
    return {};
  }
  for (const fast::Scope* scope = here; scope != nullptr;
       scope = scope->parent) {
    auto it = scope->by_name.find(name);
    if (it != scope->by_name.end()) {
      if (!hoists) {
        return scope == here && ctx_.depth == 0 ? std::string_view() : name;
      }
      const fast::Binding* b = it->second;
      const bool var_like = b->kind == fast::BindingKind::kVar ||
                            b->kind == fast::BindingKind::kParameter ||
                            b->kind == fast::BindingKind::kFunction;
      return scope->is_var_scope() && var_like ? name : std::string_view();
    }
    if (!hoists || scope->is_var_scope()) {
      break;
    }
  }
  return name;
}

bool CodeGen::ImplicitGlobalAllowed(std::string_view name) {
  // No binding of this name can be in scope. The assignment must create a
  // property of the global object. It must not store to a constant or into
  // the TDZ of a lexical binding.
  for (const redline::Variable& v : ctx_.visible) {
    if (v.name == name) {
      return false;
    }
  }
  if (!ctx_.slot.valid()) {
    return true;
  }
  fast::ScopeAnalysis& analysis = ctx_.builder.scopes();
  for (const fast::Scope* scope = analysis.ScopeOf(ctx_.slot.owner);
       scope != nullptr; scope = scope->parent) {
    if (scope->by_name.contains(name) || scope->is_dynamic()) {
      return false;
    }
  }
  return true;
}

std::vector<fast::Statement*> CodeGen::Body(GenContext& nested, int count) {
  std::vector<Statement*> body;
  if (nested.depth > nested.max_depth) {
    return body;
  }
  nested.out = &body;
  ctx_.generators.Generate(nested, count, &body);
  nested.out = nullptr;
  nested.body_used = true;
  return body;
}

REDLINE_TUNABLE(kCallSpreadChance, "codegen.call.spread_chance", 0.05, 0, 1,
                "probability that a call appends a spread of a visible array "
                "variable to its arguments");
REDLINE_TUNABLE(kCallTypedArgumentChance, "codegen.call.typed_argument_chance",
                0.8, 0, 1,
                "probability that the argument for a typed parameter has the "
                "type of the parameter");

std::vector<fast::Expression*> CodeGen::ArgumentsFor(
    const std::vector<JsType>& types) {
  Random& r = rng();
  std::vector<fast::Expression*> args;
  args.reserve(types.size());
  for (JsType type : types) {
    if (type != JsType::kUnknown && r.Chance(kCallTypedArgumentChance())) {
      // A visible variable of the type first: it gives the same value (and
      // the same map) to each call.
      fast::Identifier* id = Variable(type);
      args.push_back(id != nullptr ? static_cast<fast::Expression*>(id)
                                   : Value(type, 1));
    } else {
      args.push_back(Value(JsType::kUnknown, 1));
    }
  }
  return args;
}

fast::CallExpression* CodeGen::CallWithArguments(fast::Expression* callee,
                                                 int max_args) {
  const BindingInfo* info = nullptr;
  if (auto* id = callee->As<Identifier>()) {
    info = ctx_.builder.InfoOf(id->name);
  }
  std::vector<fast::Expression*> args =
      info != nullptr && !info->parameters.empty()
          ? ArgumentsFor(info->parameters)
          : Arguments(0, max_args);
  if (rng().Chance(kCallSpreadChance())) {
    if (fast::Identifier* id = Variable(JsType::kArray)) {
      args.push_back(mk::Spread(tree(), id));
    }
  }
  return mk::Call(tree(), callee, std::move(args));
}

}  // namespace redline
