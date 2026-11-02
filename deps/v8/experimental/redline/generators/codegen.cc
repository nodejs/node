// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/codegen.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "experimental/redline/core/tunable.h"

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
  nested.out = nullptr;
  if (is_function) {
    nested.syntax.in_function = true;
    nested.syntax.in_async = is_async;
    nested.syntax.in_generator = is_generator;
    nested.syntax.in_loop = false;
    nested.syntax.in_switch = false;
    nested.syntax.labels.clear();
    if (!is_arrow) {
      nested.syntax.in_method = false;
      nested.syntax.in_constructor = false;
      nested.syntax.has_super_class = false;
      nested.syntax.in_static_block = false;
      nested.syntax.in_class_field = false;
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
  inner.out = nullptr;
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

fast::Expression* CodeGen::Literal(JsType type) {
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
  Random& r = rng();
  // Variables first: reusing existing values creates data flow.
  if (r.Chance(kValueVariableChance())) {
    if (fast::Identifier* id = Variable(type)) {
      return id;
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
      return Construct();
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

fast::Expression* CodeGen::ObjectLiteral(int depth) {
  Random& r = rng();
  std::vector<Node*> properties;
  int n = r.SmallCount(kObjectMaxFields.AsInt(), kObjectFieldContinueChance());
  bool has_proto = false;
  for (int i = 0; i < n; i++) {
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
      GenContext nested = ctx_.Nested(true, false, false, false);
      nested.syntax.in_method = true;
      std::vector<Statement*> body = Body(
          nested, 1 + r.SmallCount(kObjectMethodMaxExtraStatements.AsInt()));
      body.push_back(mk::Return(tree(), Value(JsType::kUnknown, depth + 1)));
      FunctionExpression* fn = mk::Function(tree(), "", {}, std::move(body));
      std::string_view key = IsIdentifierName(name) ? name : "m";
      if (r.Chance(kObjectGetterChance())) {
        properties.push_back(mk::Accessor(tree(), key, fn, /*getter=*/true));
      } else {
        properties.push_back(mk::Method(tree(), key, fn));
      }
      continue;
    }
    if (r.Chance(kObjectSpreadChance())) {
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

fast::Expression* CodeGen::ArrayLiteral(int depth) {
  Random& r = rng();
  std::vector<fast::Expression*> elements;
  int n =
      r.SmallCount(kArrayMaxElements.AsInt(), kArrayElementContinueChance());
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
    } else if (r.Chance(kArraySpreadChance())) {
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

fast::Expression* CodeGen::FunctionValue(int depth) {
  Random& r = rng();
  std::vector<std::string_view> params;
  int n = static_cast<int>(r.Range(0, kFunctionMaxParams.AsInt()));
  GenContext nested = ctx_.Nested(true, /*is_arrow=*/false, false, false);
  for (int i = 0; i < n; i++) {
    std::string_view p = ctx_.builder.FreshName("a");
    params.push_back(p);
    nested.Declare(p, JsType::kUnknown);
  }
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
  CodeGen inner(nested);
  std::vector<Statement*> body;
  if (depth < ctx_.max_depth) {
    body = inner.Body(nested,
                      1 + r.SmallCount(kFunctionMaxExtraStatements.AsInt()));
  }
  if (arrow && body.empty() && r.Coin()) {
    return mk::ArrowExpr(tree(), params, inner.Expression(depth + 1), is_async);
  }
  body.push_back(mk::Return(tree(), inner.Value(JsType::kUnknown, depth + 1)));
  if (arrow) {
    return mk::Arrow(tree(), params, std::move(body), is_async);
  }
  return mk::Function(tree(), "", params, std::move(body), is_async,
                      is_generator);
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
      return mk::Id(tree(), g.name);
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

fast::Expression* CodeGen::Property(fast::Expression* object) {
  Random& r = rng();
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

std::vector<fast::Expression*> CodeGen::Arguments(
    const BuiltinFunction& function) {
  Random& r = rng();
  int n = static_cast<int>(r.Range(function.min_args, function.max_args));
  if (r.Chance(kArgumentsArityViolationChance())) {
    n = static_cast<int>(r.Range(
        0, kArgumentsArityViolationMaxArgs.AsInt()));  // arity violations
  }
  std::vector<fast::Expression*> args;
  for (int i = 0; i < n; i++) {
    JsType hint = i < 3 ? function.args[i] : JsType::kUnknown;
    if (hint == JsType::kFunction &&
        r.Chance(kArgumentsCallbackArrowChance())) {
      // Callbacks: prefer short arrows over existing functions.
      std::vector<std::string_view> params;
      int np =
          static_cast<int>(r.Range(0, kArgumentsCallbackMaxParams.AsInt()));
      GenContext nested = ctx_.Nested(true, true, false, false);
      for (int p = 0; p < np; p++) {
        std::string_view name = ctx_.builder.FreshName("e");
        params.push_back(name);
        nested.Declare(name, JsType::kUnknown);
      }
      CodeGen inner(nested);
      args.push_back(mk::ArrowExpr(tree(), params, inner.Expression(2)));
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
    return mk::Call(tree(), mk::Id(tree(), g.name), Arguments(f));
  }
  const BuiltinFunction& m = r.Pick(g.static_methods);
  return mk::CallMethod(tree(), mk::Id(tree(), g.name), m.name, Arguments(m));
}

REDLINE_TUNABLE(kMethodCallGenericChance, "codegen.method_call.generic_chance",
                0.1, 0, 1,
                "probability that a method call on a value of known type uses "
                "a generic Object method instead of one of the type's "
                "methods");

fast::Expression* CodeGen::MethodCall(fast::Expression* object, JsType type) {
  Random& r = rng();
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
    return mk::CallMethod(tree(), object, m.name, Arguments(m));
  }
  const BuiltinFunction& m = r.Pick(bt->methods);
  return mk::CallMethod(tree(), object, m.name, Arguments(m));
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
  return mk::New(tree(), mk::Id(tree(), g.name), Arguments(f));
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

fast::Expression* CodeGen::Condition(int depth) {
  Random& r = rng();
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
  ctx_.Declare(name, type, kind == VariableKind::kConst);
  return mk::Declare(tree(), kind, name, init);
}

std::vector<fast::Statement*> CodeGen::Body(GenContext& nested, int count) {
  std::vector<Statement*> body;
  if (nested.depth > nested.max_depth) {
    return body;
  }
  nested.out = &body;
  ctx_.generators.Generate(nested, count, &body);
  nested.out = nullptr;
  return body;
}

REDLINE_TUNABLE(kCallSpreadChance, "codegen.call.spread_chance", 0.05, 0, 1,
                "probability that a call appends a spread of a visible array "
                "variable to its arguments");

fast::CallExpression* CodeGen::CallWithArguments(fast::Expression* callee,
                                                 int max_args) {
  std::vector<fast::Expression*> args = Arguments(0, max_args);
  if (rng().Chance(kCallSpreadChance())) {
    if (fast::Identifier* id = Variable(JsType::kArray)) {
      args.push_back(mk::Spread(tree(), id));
    }
  }
  return mk::Call(tree(), callee, std::move(args));
}

}  // namespace redline
