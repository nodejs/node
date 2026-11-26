// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/core/adaptive_selector.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/swarm.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/core/weighted_selector.h"
#include "experimental/redline/corpus/material.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/requirements.h"
#include "experimental/redline/generators/codegen.h"
#include "experimental/redline/generators/values_generators.h"
#include "experimental/redline/mutators/explore.h"
#include "experimental/redline/mutators/fixup.h"
#include "experimental/redline/mutators/jit_compound.h"
#include "experimental/redline/mutators/mutator.h"
#include "experimental/redline/mutators/probing.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

struct MutationContext::PartnerCache {
  ProgramPtr partner;
  std::unique_ptr<Tree> donor_tree;
  std::unique_ptr<Random> donor_rng;
  std::unique_ptr<Builder> donor;
  std::vector<Node*> candidates;
};

MutationContext::MutationContext() = default;
MutationContext::~MutationContext() = default;

namespace {

// --- subtree analysis
// ------------------------------------------------------------

bool Compatible(const Requirements& req, const Builder::Context& ctx) {
  if (req.has_external_label) {
    return false;
  }
  for (std::string_view label : req.declared_labels) {
    if (std::find(ctx.labels.begin(), ctx.labels.end(), label) !=
        ctx.labels.end()) {
      return false;
    }
  }
  if (req.needs_function &&
      (!ctx.in_function || ctx.in_static_block || ctx.in_class_field)) {
    return false;
  }
  if (req.needs_async && (!ctx.in_async || ctx.in_static_block)) {
    return false;
  }
  if (req.needs_generator && !ctx.in_generator) {
    return false;
  }
  if (req.needs_break && !ctx.break_allowed()) {
    return false;
  }
  if (req.needs_continue && !ctx.in_loop) {
    return false;
  }
  if (req.needs_sloppy && ctx.strict) {
    return false;
  }
  if (req.needs_arguments && !ctx.has_arguments) {
    return false;
  }
  if (req.needs_super && !ctx.in_method) {
    return false;
  }
  if (req.needs_new_target &&
      !(ctx.has_arguments || ctx.in_class_field || ctx.in_static_block)) {
    return false;
  }
  return true;
}

// Visits two subtrees of identical shape in parallel.
template <typename F>
void ParallelWalk(Node* a, Node* b, F&& f) {
  if (a == nullptr || b == nullptr) {
    return;
  }
  f(a, b);
  std::vector<Node*> ca, cb;
  ForEachChild(a, [&](Node* c) { ca.push_back(c); });
  ForEachChild(b, [&](Node* c) { cb.push_back(c); });
  for (size_t i = 0; i < ca.size() && i < cb.size(); i++) {
    ParallelWalk(ca[i], cb[i], f);
  }
}

bool IsLexicalDeclaration(const Statement* s) {
  if (auto* v = s->As<VariableDeclaration>()) {
    return v->declaration_kind != VariableKind::kVar;
  }
  return s->Is<ClassDeclaration>();
}

bool IsDirective(const Statement* s) {
  auto* e = s->As<ExpressionStatement>();
  return e != nullptr && e->is_directive;
}

// True if `root` mentions a private name (`o.#p`, `#p in o`) without being
// the class that declares it: moved anywhere else, such code is an early
// error ("Private field must be declared in an enclosing class").
bool UsesPrivateNameOfEnclosingClass(Node* root) {
  if (root->IsClass()) {
    return false;
  }
  bool found = false;
  Walk(root, [&](Node* n) {
    if (found || n->IsClass()) {
      return false;  // a nested class declares its own names
    }
    if (n->Is<PrivateIdentifier>()) {
      found = true;
    }
    return true;
  });
  return found;
}

// True if `root` contains a reference to one of `bindings`.
bool Uses(Builder& b, Node* root, const std::vector<Binding*>& bindings) {
  if (bindings.empty()) {
    return false;
  }
  absl::flat_hash_set<const Binding*> set(bindings.begin(), bindings.end());
  bool found = false;
  Walk(root, [&](Node* n) {
    if (found) {
      return false;
    }
    auto* id = n->As<Identifier>();
    if (id != nullptr && b.scopes().IsReference(id) &&
        set.count(b.scopes().BindingOf(id))) {
      found = true;
    }
    return !found;
  });
  return found;
}

// Builds a generation context at a statement slot.
GenContext ContextForSlot(Builder& b, const Builder::Slot& slot,
                          MutationContext& ctx) {
  GenContext gen(b, *ctx.generators, *ctx.env);
  gen.slot = slot;
  gen.syntax = b.ContextAtSlot(slot);
  gen.visible = b.VisibleAtSlot(slot);
  return gen;
}

// Builds a generation context at an arbitrary node.
GenContext ContextForNode(Builder& b, Node* node, MutationContext& ctx) {
  GenContext gen(b, *ctx.generators, *ctx.env);
  Statement* enclosing = b.EnclosingStatement(node);
  if (enclosing != nullptr) {
    gen.slot = b.SlotOf(enclosing);
  }
  gen.syntax = b.ContextAt(node);
  gen.visible = b.VisibleAt(node);
  return gen;
}

Node* ReplaceAnchored(Builder& b, Node* old_node, Node* new_node) {
  if (old_node->start >= 0) {
    mk::Anchor(new_node, old_node->start);
  }
  return b.Replace(old_node, new_node) ? new_node : nullptr;
}

std::string Describe(Node* node) {
  std::string s = Print(node, fast::PrintOptions{}, nullptr);
  if (s.size() > 60) {
    s = s.substr(0, 57) + "...";
  }
  for (char& c : s) {
    if (c == '\n') {
      c = ' ';
    }
  }
  return s;
}

// --- Operation mutator
// --------------------------------------------------------------

REDLINE_TUNABLE(kOperationEmptyStringCandidateChance,
                "mutators.Operation.empty_string_candidate_chance", 0.2, 0, 1,
                "probability that an empty string literal is kept as an "
                "Operation candidate");
REDLINE_TUNABLE(kOperationMaxAttempts, "mutators.Operation.max_attempts", 8, 1,
                16,
                "candidate nodes the Operation mutator tries before giving up");
REDLINE_TUNABLE(
    kOperationBinaryOpAttempts, "mutators.Operation.binary_op_attempts", 4, 1,
    16, "draws to find a binary operator different from the current one");
REDLINE_TUNABLE(kOperationPlainAssignChance,
                "mutators.Operation.plain_assign_chance", 0.2, 0, 1,
                "probability that a mutated assignment operator becomes plain "
                "`=` rather than a compound one");
REDLINE_TUNABLE(kOperationRandomNumberMax,
                "mutators.Operation.random_number_max", 100, 1, 1024,
                "upper bound of the uniform random replacement for a numeric "
                "literal (0..max)");
REDLINE_TUNABLE(kOperationPropertyNameAttempts,
                "mutators.Operation.property_name_attempts", 4, 1, 16,
                "draws to find a property name different from the current one");
REDLINE_TUNABLE(kOperationCharEditChance, "mutators.Operation.char_edit_chance",
                0.4, 0, 1,
                "probability that a string literal gets a one-character edit "
                "(add, remove or replace) rather than a new value");
REDLINE_TUNABLE(kOperationOptionalChainChance,
                "mutators.Operation.optional_chain_chance", 0.2, 0, 1,
                "probability that a member expression toggles optional "
                "chaining instead of changing its property key");
REDLINE_TUNABLE(kOperationMemberKnownKeyChance,
                "mutators.Operation.member_known_key_chance", 0.4, 0, 1,
                "probability that a member key edit picks a known key of the "
                "object variable");
REDLINE_TUNABLE(kOperationMemberComputedKeyChance,
                "mutators.Operation.member_computed_key_chance", 0.25, 0, 1,
                "probability that a member key edit picks a numeric index or a "
                "well-known symbol");
REDLINE_TUNABLE(kOperationRegExpFlagChance,
                "mutators.Operation.regexp_flag_chance", 0.35, 0, 1,
                "probability that a RegExp edit changes flags instead of the "
                "pattern");
REDLINE_TUNABLE(kOperationRegExpPieceEditChance,
                "mutators.Operation.regexp_piece_edit_chance", 0.5, 0, 1,
                "probability that a RegExp pattern edit mutates the existing "
                "pattern in place rather than generating a fresh pattern");
REDLINE_TUNABLE(kOperationRegExpMaxPatternLength,
                "mutators.Operation.regexp_max_pattern_length", 128, 16, 1024,
                "largest RegExp pattern length that in-place wrapping or "
                "concatenation extends");
REDLINE_TUNABLE(kOperationBigIntSignChance,
                "mutators.Operation.bigint_sign_chance", 0.2, 0, 1,
                "probability that a BigInt literal edit toggles its sign when "
                "safe");

// Regular expression flags that do not change pattern syntax.
constexpr std::string_view kSyntaxNeutralRegExpFlags = "dgimsy";

// Walks up the member and call spine to the outermost chain expression.
Expression* ChainTop(Builder& b, Expression* node) {
  Expression* cur = node;
  while (true) {
    Node* up = b.ParentOf(cur);
    if (up == nullptr) {
      break;
    }
    if (auto* m = up->As<MemberExpression>();
        m != nullptr && m->object == cur) {
      cur = m;
      continue;
    }
    if (auto* c = up->As<CallExpression>(); c != nullptr && c->callee == cur) {
      cur = c;
      continue;
    }
    if (auto* ch = up->As<ChainExpression>();
        ch != nullptr && ch->expression == cur) {
      return ch;
    }
    break;
  }
  return cur;
}

// Returns true if any member or call on the spine of `expr` is optional.
bool SpineHasOptional(const Expression* expr) {
  const Expression* cur = expr;
  while (cur != nullptr) {
    if (const auto* m = cur->As<MemberExpression>()) {
      if (m->optional) {
        return true;
      }
      cur = m->object;
    } else if (const auto* c = cur->As<CallExpression>()) {
      if (c->optional) {
        return true;
      }
      cur = c->callee;
    } else {
      break;
    }
  }
  return false;
}

// Removes `chain` when no optional link remains on its spine.
void UnwrapEmptyChain(Builder& b, ChainExpression* chain) {
  if (chain != nullptr && chain->expression != nullptr &&
      !SpineHasOptional(chain->expression)) {
    ReplaceAnchored(b, chain, chain->expression);
  }
}

// Returns true when `node` can safely toggle `?.` on or off.
bool CanToggleOptionalChain(Builder& b, Expression* node) {
  bool currently_optional = false;
  bool is_private = false;
  if (auto* m = node->As<MemberExpression>()) {
    if (m->object->Is<Super>()) {
      return false;
    }
    currently_optional = m->optional;
    is_private = m->property->Is<PrivateIdentifier>();
  } else if (auto* c = node->As<CallExpression>()) {
    if (c->callee->Is<Super>()) {
      return false;
    }
    currently_optional = c->optional;
  } else {
    return false;
  }
  Expression* top = ChainTop(b, node);
  Node* owner = b.ParentOf(top);
  if (owner == nullptr) {
    return false;
  }
  if (currently_optional) {
    return true;
  }
  if (owner->Is<AssignmentExpression>() &&
      owner->Cast<AssignmentExpression>()->left == top) {
    return false;
  }
  if (owner->Is<UpdateExpression>()) {
    return false;
  }
  if (owner->Is<ForInStatement>() &&
      owner->Cast<ForInStatement>()->left == top) {
    return false;
  }
  if (owner->Is<ForOfStatement>() &&
      owner->Cast<ForOfStatement>()->left == top) {
    return false;
  }
  if (owner->Is<NewExpression>() &&
      owner->Cast<NewExpression>()->callee == top) {
    return false;
  }
  if (owner->Is<TaggedTemplateExpression>() &&
      owner->Cast<TaggedTemplateExpression>()->tag == top) {
    return false;
  }
  if (owner->Is<ObjectPattern>() || owner->Is<ArrayPattern>() ||
      owner->Is<RestElement>()) {
    return false;
  }
  if (owner->Is<AssignmentPattern>() &&
      owner->Cast<AssignmentPattern>()->left == top) {
    return false;
  }
  if (owner->Is<Property>() && owner->Cast<Property>()->value == top) {
    Node* container = b.ParentOf(owner);
    if (container != nullptr && container->Is<ObjectPattern>()) {
      return false;
    }
  }
  if (is_private && owner->Is<UnaryExpression>() &&
      owner->Cast<UnaryExpression>()->op == UnaryOp::kDelete) {
    return false;
  }
  return true;
}

// Toggles optional chaining on a `MemberExpression` or `CallExpression`.
bool ToggleOptionalChain(Builder& b, Expression* node, MutationContext& ctx) {
  if (!CanToggleOptionalChain(b, node)) {
    return false;
  }
  Expression* top = ChainTop(b, node);
  auto* m = node->As<MemberExpression>();
  auto* c = node->As<CallExpression>();
  const bool was_optional = m != nullptr ? m->optional : c->optional;
  if (!was_optional && !top->Is<ChainExpression>() &&
      ReplaceAnchored(b, top, mk::Chain(b.tree(), top)) == nullptr) {
    return false;
  }
  if (m != nullptr) {
    m->optional = !was_optional;
    ctx.detail = was_optional ? "optional member -> member"
                              : "member -> optional member";
  } else {
    c->optional = !was_optional;
    ctx.detail =
        was_optional ? "optional call -> call" : "call -> optional call";
  }
  if (was_optional) {
    if (auto* chain = top->As<ChainExpression>()) {
      UnwrapEmptyChain(b, chain);
    }
  }
  b.Invalidate();
  return true;
}

// Parses an unsigned `BigIntLiteral` raw string into `out` when it fits in
// 64 bits.
bool ParseBigIntUint64(std::string_view raw, uint64_t* out) {
  if (raw.size() < 2 || raw.back() != 'n') {
    return false;
  }
  std::string_view digits = raw.substr(0, raw.size() - 1);
  uint64_t base = 10;
  if (digits.size() >= 2 && digits[0] == '0') {
    char p = digits[1];
    if (p == 'x' || p == 'X') {
      base = 16;
      digits = digits.substr(2);
    } else if (p == 'b' || p == 'B') {
      base = 2;
      digits = digits.substr(2);
    } else if (p == 'o' || p == 'O') {
      base = 8;
      digits = digits.substr(2);
    }
  }
  if (digits.empty()) {
    return false;
  }
  uint64_t val = 0;
  bool any_digit = false;
  for (char c : digits) {
    if (c == '_') {
      continue;
    }
    uint64_t d = 0;
    if (c >= '0' && c <= '9') {
      d = static_cast<uint64_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      d = static_cast<uint64_t>(10 + (c - 'a'));
    } else if (c >= 'A' && c <= 'F') {
      d = static_cast<uint64_t>(10 + (c - 'A'));
    } else {
      return false;
    }
    if (d >= base) {
      return false;
    }
    if (val > (std::numeric_limits<uint64_t>::max() - d) / base) {
      return false;
    }
    val = val * base + d;
    any_digit = true;
  }
  if (!any_digit) {
    return false;
  }
  *out = val;
  return true;
}

// Returns true when `node` sits in a non-computed property key slot.
bool IsNonComputedPropertyKey(Builder& b, Node* node) {
  Node* parent = b.ParentOf(node);
  if (parent == nullptr) {
    return false;
  }
  if (auto* p = parent->As<Property>()) {
    return p->key == node && !p->computed;
  }
  if (auto* m = parent->As<MethodDefinition>()) {
    return m->key == node && !m->computed;
  }
  if (auto* f = parent->As<PropertyDefinition>()) {
    return f->key == node && !f->computed;
  }
  return false;
}

// Returns true when `pattern` is valid in non-unicode, `u`, and `v` modes.
bool IsModeNeutralRegExpPattern(std::string_view pattern) {
  if (pattern.empty()) {
    return false;
  }
  char prev = '\0';
  for (size_t i = 0; i < pattern.size(); i++) {
    char c = pattern[i];
    if (c == '\\') {
      if (i + 1 >= pattern.size()) {
        return false;
      }
      char next = pattern[++i];
      if (next != 'd' && next != 'D' && next != 'w' && next != 'W' &&
          next != 's' && next != 'S') {
        return false;
      }
      prev = next;
      continue;
    }
    if ((c == '*' || c == '+' || c == '?') && (prev == '^' || prev == '$')) {
      return false;
    }
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == ' ' || c == '.' ||
              c == '|' || c == '*' || c == '+' || c == '?' || c == '^' ||
              c == '$' || c == ',' || c == ':' || c == ';' || c == '!' ||
              c == '@' || c == '#' || c == '%' || c == '=' || c == '<' ||
              c == '>' || c == '~' || c == '\'' || c == '"';
    if (!ok) {
      return false;
    }
    prev = c;
  }
  return true;
}

// Returns true when `suffix` is empty or a single RegExp quantifier.
bool IsRegExpQuantifierSuffix(std::string_view suffix) {
  if (suffix.empty() || suffix == "*" || suffix == "+" || suffix == "?" ||
      suffix == "*?" || suffix == "+?" || suffix == "??") {
    return true;
  }
  if (suffix.size() < 3 || suffix.front() != '{') {
    return false;
  }
  size_t end = suffix.back() == '?' ? suffix.size() - 1 : suffix.size();
  if (end < 3 || suffix[end - 1] != '}') {
    return false;
  }
  std::string_view body = suffix.substr(1, end - 2);
  size_t comma = body.find(',');
  auto all_digits = [](std::string_view s) {
    if (s.empty()) {
      return false;
    }
    for (char c : s) {
      if (c < '0' || c > '9') {
        return false;
      }
    }
    return true;
  };
  if (comma == std::string_view::npos) {
    return all_digits(body);
  }
  std::string_view lo = body.substr(0, comma);
  std::string_view hi = body.substr(comma + 1);
  return all_digits(lo) && (hi.empty() || all_digits(hi));
}

// Unwraps an outer `(?:...)` group with an optional trailing quantifier.
bool UnwrapOuterNonCapturingGroup(std::string_view pattern, bool sets,
                                  std::string* inner, std::string* suffix) {
  if (pattern.size() < 4 || pattern.substr(0, 3) != "(?:") {
    return false;
  }
  int paren_depth = 0;
  int class_depth = 0;
  size_t close_pos = std::string_view::npos;
  for (size_t i = 0; i < pattern.size(); i++) {
    char c = pattern[i];
    if (c == '\\') {
      i++;
      continue;
    }
    if (class_depth > 0) {
      if (sets && c == '[') {
        class_depth++;
      } else if (c == ']') {
        class_depth--;
      }
      continue;
    }
    if (c == '[') {
      class_depth = 1;
    } else if (c == '(') {
      paren_depth++;
    } else if (c == ')') {
      paren_depth--;
      if (paren_depth == 0) {
        close_pos = i;
        break;
      }
    }
  }
  if (close_pos == std::string_view::npos) {
    return false;
  }
  std::string_view body = pattern.substr(3, close_pos - 3);
  std::string_view rest = pattern.substr(close_pos + 1);
  if (body.empty() || !IsRegExpQuantifierSuffix(rest)) {
    return false;
  }
  *inner = std::string(body);
  *suffix = std::string(rest);
  return true;
}

bool HasUnescapedTrailingDollar(std::string_view pattern) {
  if (pattern.size() <= 1 || pattern.back() != '$') {
    return false;
  }
  size_t slashes = 0;
  for (size_t i = pattern.size() - 1; i > 0 && pattern[i - 1] == '\\'; i--) {
    slashes++;
  }
  return (slashes % 2) == 0;
}

// Toggles one safe literal letter case, shorthand escape, or character class
// negation inside `pattern`.
bool ToggleRegExpCaseOrClass(Random& r, std::string_view pattern, bool sets,
                             std::string* out) {
  enum class EditKind { kFlipCase, kToggleClassNegation };
  struct Edit {
    EditKind kind;
    size_t pos;
  };
  std::vector<Edit> edits;
  int class_depth = 0;
  size_t class_start = 0;
  bool class_simple = false;
  bool class_has_body = false;
  for (size_t i = 0; i < pattern.size(); i++) {
    char c = pattern[i];
    if (c == '\\') {
      if (i + 1 >= pattern.size()) {
        break;
      }
      char next = pattern[i + 1];
      if (class_depth > 0) {
        class_has_body = true;
        if (next == 'q' || next == 'p' || next == 'P') {
          class_simple = false;
        }
        if (next == 'd' || next == 'D' || next == 'w' || next == 'W' ||
            next == 's' || next == 'S') {
          edits.push_back({EditKind::kFlipCase, i + 1});
          i++;
          continue;
        }
        if ((next == 'p' || next == 'P' || next == 'u' || next == 'q') &&
            i + 2 < pattern.size() && pattern[i + 2] == '{') {
          size_t close = pattern.find('}', i + 3);
          i = close == std::string_view::npos ? pattern.size() - 1 : close;
          continue;
        }
        i++;
        continue;
      }
      if (next == 'd' || next == 'D' || next == 'w' || next == 'W' ||
          next == 's' || next == 'S' || next == 'b' || next == 'B') {
        edits.push_back({EditKind::kFlipCase, i + 1});
        i++;
        continue;
      }
      if (next == 'p' || next == 'P' || next == 'u' || next == 'q') {
        if (i + 2 < pattern.size() && pattern[i + 2] == '{') {
          size_t close = pattern.find('}', i + 3);
          i = close == std::string_view::npos ? pattern.size() - 1 : close;
          continue;
        }
        if (next == 'u') {
          i = std::min(i + 5, pattern.size() - 1);
          continue;
        }
      } else if (next == 'k') {
        if (i + 2 < pattern.size() && pattern[i + 2] == '<') {
          size_t close = pattern.find('>', i + 3);
          i = close == std::string_view::npos ? pattern.size() - 1 : close;
          continue;
        }
      } else if (next == 'x') {
        i = std::min(i + 3, pattern.size() - 1);
        continue;
      } else if (next == 'c') {
        i = std::min(i + 2, pattern.size() - 1);
        continue;
      }
      i++;
      continue;
    }
    if (class_depth > 0) {
      if (sets && c == '[') {
        class_depth++;
        class_simple = false;
        continue;
      }
      if (c == ']') {
        class_depth--;
        if (class_depth == 0 && class_simple && class_has_body) {
          edits.push_back({EditKind::kToggleClassNegation, class_start});
        }
        continue;
      }
      if (i == class_start + 1 && c == '^') {
        continue;
      }
      class_has_body = true;
      continue;
    }
    if (c == '[') {
      class_depth = 1;
      class_start = i;
      class_simple = true;
      class_has_body = false;
      continue;
    }
    if (c == '(' && i + 1 < pattern.size() && pattern[i + 1] == '?') {
      size_t j = i + 2;
      if (j < pattern.size() && pattern[j] == '<') {
        j++;
        if (j < pattern.size() && (pattern[j] == '=' || pattern[j] == '!')) {
          i = j;
        } else {
          size_t close = pattern.find('>', j);
          i = close == std::string_view::npos ? pattern.size() - 1 : close;
        }
        continue;
      }
      while (j < pattern.size() && pattern[j] != ':' && pattern[j] != '=' &&
             pattern[j] != '!' && pattern[j] != ')') {
        j++;
      }
      i = j < pattern.size() ? j : pattern.size() - 1;
      continue;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      edits.push_back({EditKind::kFlipCase, i});
    }
  }
  if (edits.empty()) {
    return false;
  }
  const Edit edit = r.Pick(edits);
  *out = std::string(pattern);
  if (edit.kind == EditKind::kFlipCase) {
    (*out)[edit.pos] = static_cast<char>((*out)[edit.pos] ^ 0x20);
  } else {
    if (edit.pos + 1 < out->size() && (*out)[edit.pos + 1] == '^') {
      out->erase(edit.pos + 1, 1);
    } else {
      out->insert(edit.pos + 1, 1, '^');
    }
  }
  return true;
}

// Applies a valid-by-construction in-place edit to a RegExp pattern.
bool MutateRegExpPatternInPlace(Random& r, std::string_view pattern,
                                bool unicode, bool sets, std::string* out,
                                std::string* what) {
  if (pattern.empty()) {
    return false;
  }
  const size_t max_len =
      static_cast<size_t>(kOperationRegExpMaxPatternLength.AsInt());
  switch (r.Index(4)) {
    case 0: {
      // Wrap or unwrap a non-capturing group and quantifier.
      std::string inner, suffix;
      if (UnwrapOuterNonCapturingGroup(pattern, sets, &inner, &suffix) &&
          r.Coin()) {
        if (suffix.empty() || r.Coin()) {
          *out = std::move(inner);
          *what = "unwrapped group";
        } else {
          std::string_view q =
              r.Pick({"*", "+", "?", "{1,2}", "*?", "+?", "??", "{1,2}?"});
          *out = "(?:" + inner + ")" + std::string(q);
          *what = "changed quantifier";
        }
        return *out != pattern;
      }
      if (pattern.size() <= max_len) {
        if (r.Index(5) == 0) {
          std::string_view mod = r.Pick({"(?i:", "(?-i:", "(?m:", "(?s:"});
          *out = std::string(mod) + std::string(pattern) + ")";
          *what = "wrapped modifier group";
        } else {
          std::string_view q =
              r.Pick({"*", "+", "?", "{1,2}", "*?", "+?", "??", "{1,2}?"});
          *out = "(?:" + std::string(pattern) + ")" + std::string(q);
          *what = "wrapped quantifier";
        }
        return true;
      }
      break;
    }
    case 1: {
      // Toggle leading `^` or trailing `$` anchor.
      bool has_start = pattern.size() > 1 && pattern.front() == '^' &&
                       pattern[1] != '*' && pattern[1] != '+' &&
                       pattern[1] != '?' && pattern[1] != '{';
      bool has_end = HasUnescapedTrailingDollar(pattern);
      if ((has_start || has_end) && r.Coin()) {
        if (has_start && (!has_end || r.Coin())) {
          *out = std::string(pattern.substr(1));
        } else {
          *out = std::string(pattern.substr(0, pattern.size() - 1));
        }
        *what = "removed anchor";
        return !out->empty();
      }
      if (pattern.size() <= max_len) {
        switch (r.Index(3)) {
          case 0:
            *out = "^(?:" + std::string(pattern) + ")";
            break;
          case 1:
            *out = "(?:" + std::string(pattern) + ")$";
            break;
          default:
            *out = "^(?:" + std::string(pattern) + ")$";
            break;
        }
        *what = "added anchor";
        return true;
      }
      break;
    }
    case 2:
      if (ToggleRegExpCaseOrClass(r, pattern, sets, out)) {
        *what = "toggled case or class";
        return true;
      }
      break;
    default:
      break;
  }
  std::string piece = CodeGen::RandomRegExpPiece(r, unicode, sets);
  if (pattern.size() <= max_len && r.Index(3) != 0) {
    switch (r.Index(3)) {
      case 0:
        *out = "(?:" + std::string(pattern) + ")|(?:" + piece + ")";
        *what = "added alternative";
        break;
      case 1:
        *out = "(?:" + std::string(pattern) + ")(?:" + piece + ")";
        *what = "appended piece";
        break;
      default:
        *out = "(?:" + piece + ")(?:" + std::string(pattern) + ")";
        *what = "prepended piece";
        break;
    }
    return true;
  }
  *out = std::move(piece);
  *what = "substituted piece";
  return !out->empty() && *out != pattern;
}

// Converts a property name into a valid member key expression and `computed`
// flag. Returns nullptr when the name needs brackets and `allow_computed` is
// false.
Expression* MakeMemberProperty(Tree* t, std::string_view name,
                               bool allow_computed, bool* computed) {
  if (IsIdentifierName(name)) {
    *computed = false;
    return mk::Id(t, name);
  }
  if (!allow_computed) {
    return nullptr;
  }
  *computed = true;
  if (name.starts_with("Symbol.") && IsIdentifierName(name.substr(7))) {
    return mk::Member(t, mk::Id(t, "Symbol"), name.substr(7));
  }
  bool numeric = !name.empty();
  for (char c : name) {
    if (!((c >= '0' && c <= '9') || c == '.' || c == '-')) {
      numeric = false;
      break;
    }
  }
  if (numeric) {
    std::string text(name);
    char* end = nullptr;
    double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() + text.size() && std::isfinite(value)) {
      return mk::Num(t, value);
    }
  }
  return mk::Str(t, name);
}

// Returns true when `expr` has an iterable or unknown guessed type.
bool IsSpreadPreferredArg(Builder& b, const Expression* expr) {
  if (expr->Is<SpreadElement>()) {
    return true;
  }
  JsType type = b.GuessExpressionType(expr);
  switch (type) {
    case JsType::kArray:
    case JsType::kString:
    case JsType::kTypedArray:
    case JsType::kMap:
    case JsType::kSet:
    case JsType::kIterator:
    case JsType::kUnknown:
      return true;
    default:
      return false;
  }
}

// A printable ASCII character for a one-character string edit.
char RandomStringChar(Random& r) {
  constexpr std::string_view kChars =
      "abcxyzABCXYZ0189 _-.,:/\\'\"`$#@!?%&*+=<>()[]{}|~^";
  return kChars[r.Index(kChars.size())];
}

// True if `pos` is not inside a multi-byte WTF-8 sequence of `s`.
bool IsCharBoundary(const std::string& s, size_t pos) {
  return pos >= s.size() || (static_cast<unsigned char>(s[pos]) & 0xC0) != 0x80;
}

class OperationMutator final : public Mutator {
 public:
  // Fuzzilli: OperationMutator, weight 1, up to 7 simultaneous.
  OperationMutator()
      : Mutator("Operation",
                "changes an operator, literal value or property name in place",
                1, false, 7) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      switch (n->kind) {
        case NodeKind::kBinaryExpression:
        case NodeKind::kLogicalExpression:
        case NodeKind::kUnaryExpression:
        case NodeKind::kUpdateExpression:
        case NodeKind::kAssignmentExpression:
        case NodeKind::kNumericLiteral:
        case NodeKind::kBigIntLiteral:
        case NodeKind::kBooleanLiteral:
        case NodeKind::kMemberExpression:
        case NodeKind::kRegExpLiteral:
        case NodeKind::kIfStatement:
          return true;
        case NodeKind::kStringLiteral:
          return !n->Cast<StringLiteral>()->value.empty() ||
                 r.Chance(kOperationEmptyStringCandidateChance());
        default:
          return false;
      }
    });
    for (int attempt = 0;
         attempt < kOperationMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      Node* node = candidates[r.Index(candidates.size())];
      if (MutateNode(b, node, ctx)) {
        return true;
      }
    }
    return false;
  }

 private:
  bool MutateNode(Builder& b, Node* node, MutationContext& ctx) {
    Random& r = b.rng();
    Tree* t = b.tree();
    switch (node->kind) {
      case NodeKind::kBinaryExpression: {
        auto* e = node->Cast<BinaryExpression>();
        if (e->left->Is<PrivateIdentifier>()) {
          return false;
        }
        BinaryOp op = e->op;
        for (int i = 0; i < kOperationBinaryOpAttempts.AsInt() && op == e->op;
             i++) {
          op = CodeGen::RandomBinaryOp(r);
        }
        if (op == BinaryOp::kExp && (e->left->Is<UnaryExpression>() ||
                                     e->left->Is<AwaitExpression>())) {
          return false;
        }
        ctx.detail = std::string("binary ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kLogicalExpression: {
        auto* e = node->Cast<LogicalExpression>();
        LogicalOp op = e->op;
        while (op == e->op) {
          op = CodeGen::RandomLogicalOp(r);
        }
        // `a ?? b || c` without parentheses is an error; the printer adds
        // them when precedences mix, so this is safe.
        ctx.detail = std::string("logical ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kUnaryExpression: {
        auto* e = node->Cast<UnaryExpression>();
        if (e->op == UnaryOp::kDelete) {
          return false;
        }
        UnaryOp op = e->op;
        while (op == e->op) {
          op = CodeGen::RandomUnaryOp(r);
        }
        Node* parent = b.ParentOf(node);
        if (parent != nullptr && parent->Is<BinaryExpression>() &&
            parent->Cast<BinaryExpression>()->op == BinaryOp::kExp &&
            parent->Cast<BinaryExpression>()->left == node) {
          return false;  // `-x ** 2`
        }
        ctx.detail = std::string("unary ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kUpdateExpression: {
        auto* e = node->Cast<UpdateExpression>();
        if (r.Coin()) {
          e->op = e->op == UpdateOp::kIncrement ? UpdateOp::kDecrement
                                                : UpdateOp::kIncrement;
          ctx.detail = "update operator flipped";
        } else {
          e->prefix = !e->prefix;
          ctx.detail = "update prefix flipped";
        }
        return true;
      }
      case NodeKind::kAssignmentExpression: {
        auto* e = node->Cast<AssignmentExpression>();
        if (!e->left->Is<Identifier>() && !e->left->Is<MemberExpression>()) {
          return false;
        }
        AssignOp op = r.Chance(kOperationPlainAssignChance())
                          ? AssignOp::kAssign
                          : CodeGen::RandomCompoundAssignOp(r);
        if (op == e->op) {
          return false;
        }
        ctx.detail = std::string("assignment ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kNumericLiteral: {
        auto* n = node->Cast<NumericLiteral>();
        double old = n->value;
        double v;  // Assigned on every path of the switch below.
        switch (r.Index(8)) {
          case 0:
            v = old + 1;
            break;
          case 1:
            v = old - 1;
            break;
          case 2:
            v = -old;
            break;
          case 3:
            v = old * 2;
            break;
          case 4:
            v = r.Pick(ctx.env->numbers());
            break;
          case 5:
            v = static_cast<double>(
                r.Range(0, kOperationRandomNumberMax.AsInt()));
            break;
          case 6:
            v = old / 2;
            break;
          default:
            v = static_cast<double>(r.Next() >> 32);
            break;
        }
        if (v == old) {
          return false;
        }
        // Negative numbers (including `-0`) as property keys are not literals.
        const bool is_key = IsNonComputedPropertyKey(b, node);
        if (is_key && (v < 0 || std::signbit(v) || v != v || std::isinf(v))) {
          v = std::abs(v == v ? v : 1);
        }
        if (is_key && std::isinf(v)) {
          v = 1;
        }
        n->value = v;
        n->raw = {};
        ctx.detail = StringPrintf("number %g -> %g", old, v);
        return true;
      }
      case NodeKind::kBigIntLiteral: {
        static constexpr std::string_view kBoundaries[] = {
            "0n",
            "1n",
            "2n",
            "3n",
            "8n",
            "16n",
            "31n",
            "32n",
            "63n",
            "64n",
            "255n",
            "256n",
            "65535n",
            "65536n",
            "1073741823n",
            "1073741824n",
            "2147483647n",
            "2147483648n",
            "4294967295n",
            "4294967296n",
            "9007199254740991n",
            "9007199254740992n",
            "9007199254740993n",
            "9223372036854775807n",
            "9223372036854775808n",
            "18446744073709551615n",
            "18446744073709551616n",
            "340282366920938463463374607431768211456n",
        };
        auto* lit = node->Cast<BigIntLiteral>();
        Node* parent = b.ParentOf(node);
        const bool is_key = IsNonComputedPropertyKey(b, node);
        if (r.Chance(kOperationBigIntSignChance())) {
          if (parent != nullptr && parent->Is<UnaryExpression>() &&
              parent->Cast<UnaryExpression>()->op == UnaryOp::kMinus &&
              b.ParentOf(parent) != nullptr &&
              b.RoleOf(parent) == ExpressionRole::kValue) {
            std::string raw(lit->raw);
            if (ReplaceAnchored(b, parent, lit) != nullptr) {
              ctx.detail = "bigint sign -" + raw + " -> " + raw;
              return true;
            }
          } else if (!is_key && lit->raw != "0n" &&
                     b.RoleOf(node) == ExpressionRole::kValue &&
                     (parent == nullptr || !parent->Is<UnaryExpression>()) &&
                     !(parent != nullptr && parent->Is<BinaryExpression>() &&
                       parent->Cast<BinaryExpression>()->op == BinaryOp::kExp &&
                       parent->Cast<BinaryExpression>()->left == node)) {
            std::string raw(lit->raw);
            if (ReplaceAnchored(b, lit, mk::Unary(t, UnaryOp::kMinus, lit)) !=
                nullptr) {
              ctx.detail = "bigint sign " + raw + " -> -" + raw;
              return true;
            }
          }
        }
        uint64_t val = 0;
        const bool fits_u64 = ParseBigIntUint64(lit->raw, &val);
        std::string next;
        if (!fits_u64) {
          next = r.Coin() ? std::string(kBoundaries[r.Index(
                                sizeof(kBoundaries) / sizeof(kBoundaries[0]))])
                          : std::to_string(r.Next()) + "n";
        } else {
          switch (r.Index(7)) {
            case 0:
            case 1:
              next = std::string(kBoundaries[r.Index(sizeof(kBoundaries) /
                                                     sizeof(kBoundaries[0]))]);
              break;
            case 2:
              next = val == std::numeric_limits<uint64_t>::max()
                         ? "18446744073709551616n"
                         : std::to_string(val + 1) + "n";
              break;
            case 3:
              next = std::to_string(val > 0 ? val - 1 : 1) + "n";
              break;
            case 4:
              next = std::to_string(val ^ (uint64_t{1} << r.Index(64))) + "n";
              break;
            case 5: {
              const unsigned shift = static_cast<unsigned>(r.Range(1, 16));
              const uint64_t shifted =
                  r.Coin() ? (val == 0 ? (uint64_t{1} << r.Index(64))
                                       : (val << shift))
                           : (val >> shift);
              next = std::to_string(shifted) + "n";
              break;
            }
            default:
              next = std::to_string(
                         r.Coin() ? static_cast<uint64_t>(r.Range(
                                        0, kOperationRandomNumberMax.AsInt()))
                                  : r.Next()) +
                     "n";
              break;
          }
        }
        if (next == lit->raw) {
          return false;
        }
        ctx.detail = "bigint " + std::string(lit->raw) + " -> " + next;
        lit->raw = t->Intern(next);
        return true;
      }
      case NodeKind::kBooleanLiteral: {
        auto* lit = node->Cast<BooleanLiteral>();
        lit->value = !lit->value;
        ctx.detail = "boolean flipped";
        return true;
      }
      case NodeKind::kStringLiteral: {
        auto* s = node->Cast<StringLiteral>();
        Node* parent = b.ParentOf(node);
        if (parent != nullptr && parent->Is<ExpressionStatement>() &&
            parent->Cast<ExpressionStatement>()->is_directive) {
          return false;
        }
        if (parent != nullptr && parent->Is<ImportExpression>()) {
          return false;
        }
        std::string old = s->value;
        std::string v;
        if (r.Chance(kOperationCharEditChance())) {
          // One character added, removed or replaced (Fuzzilli's
          // OperationMutator edits strings the same way).
          v = old;
          const size_t pos = r.Index(old.size() + 1);
          const size_t action = old.empty() ? 0 : r.Index(3);
          if (!IsCharBoundary(old, pos)) {
            return false;
          }
          if (action == 0) {
            v.insert(pos, 1, RandomStringChar(r));
          } else if (pos >= old.size() ||
                     static_cast<unsigned char>(old[pos]) >= 0x80) {
            return false;  // Only ASCII characters are removed or replaced.
          } else if (action == 1) {
            v.erase(pos, 1);
          } else {
            v[pos] = RandomStringChar(r);
          }
        } else {
          switch (r.Index(5)) {
            case 0:
              v = std::string(r.Pick(ctx.env->strings()));
              break;
            case 1:
              v = std::string(r.Pick(ctx.env->property_names()));
              break;
            case 2:
              v = old + old;
              break;
            case 3:
              v = old.substr(0, old.size() / 2);
              break;
            default:
              v = old + "x";
              break;
          }
        }
        if (v == old) {
          return false;
        }
        s->value = v;
        s->raw = {};
        ctx.detail =
            "string \"" + (old.size() > 20 ? old.substr(0, 20) + "..." : old) +
            "\" -> \"" + (v.size() > 20 ? v.substr(0, 20) + "..." : v) + "\"";
        return true;
      }
      case NodeKind::kMemberExpression: {
        auto* m = node->Cast<MemberExpression>();
        if (m->property->Is<PrivateIdentifier>()) {
          return ToggleOptionalChain(b, m, ctx);
        }
        if (r.Chance(kOperationOptionalChainChance()) &&
            ToggleOptionalChain(b, m, ctx)) {
          return true;
        }
        const bool allow_computed =
            !(m->object->Is<Identifier>() &&
              m->object->Cast<Identifier>()->name == "let");
        const std::string old_desc = Describe(m->property);
        const bool old_computed = m->computed;
        const BindingInfo* info = nullptr;
        if (auto* obj_id = m->object->As<Identifier>()) {
          info = b.InfoOf(obj_id->name);
        }
        JsType type = b.GuessExpressionType(m->object);
        const BuiltinType* bt = ctx.env->ForType(type);
        static constexpr std::string_view kWellKnownSymbols[] = {
            "iterator",
            "asyncIterator",
            "toPrimitive",
            "hasInstance",
            "species",
            "toStringTag",
            "isConcatSpreadable",
            "match",
            "replace",
            "search",
            "split",
            "unscopables",
            "dispose",
            "asyncDispose"};
        static constexpr double kInterestingIndices[] = {
            0, 1, 2, 3, -1, 10, 100, 4294967295};
        Expression* new_prop = nullptr;
        bool new_computed = false;
        for (int i = 0; i < kOperationPropertyNameAttempts.AsInt(); i++) {
          Expression* cand = nullptr;
          bool cand_computed = false;
          if (info != nullptr && !info->keys.empty() &&
              r.Chance(kOperationMemberKnownKeyChance())) {
            const KnownKey& key = r.Pick(info->keys);
            cand =
                MakeMemberProperty(t, key.name, allow_computed, &cand_computed);
          } else if (allow_computed &&
                     r.Chance(kOperationMemberComputedKeyChance())) {
            cand_computed = true;
            if (r.Coin()) {
              double idx = r.Coin() ? kInterestingIndices[r.Index(
                                          sizeof(kInterestingIndices) /
                                          sizeof(kInterestingIndices[0]))]
                                    : static_cast<double>(r.Range(0, 15));
              cand = mk::Num(t, idx);
            } else {
              std::string_view sym = kWellKnownSymbols[r.Index(
                  sizeof(kWellKnownSymbols) / sizeof(kWellKnownSymbols[0]))];
              cand = mk::Member(t, mk::Id(t, "Symbol"), sym);
            }
          } else {
            std::string_view name;
            if (bt != nullptr && r.Coin()) {
              if (!bt->methods.empty() && r.Coin()) {
                name = r.Pick(bt->methods).name;
              } else if (!bt->properties.empty()) {
                name = r.Pick(bt->properties);
              }
            }
            if (name.empty()) {
              name = r.Pick(ctx.env->property_names());
            }
            cand = MakeMemberProperty(t, name, allow_computed, &cand_computed);
          }
          if (cand == nullptr) {
            continue;
          }
          if (cand_computed != old_computed || Describe(cand) != old_desc) {
            new_prop = cand;
            new_computed = cand_computed;
            break;
          }
        }
        if (new_prop == nullptr) {
          return ToggleOptionalChain(b, m, ctx);
        }
        if (m->start >= 0) {
          mk::Anchor(new_prop, m->start);
        }
        m->computed = new_computed;
        m->property = new_prop;
        b.Invalidate();
        ctx.detail = "property " +
                     (old_computed ? "[" + old_desc + "]" : "." + old_desc) +
                     " -> " +
                     (new_computed ? "[" + Describe(new_prop) + "]"
                                   : "." + Describe(new_prop));
        return true;
      }
      case NodeKind::kRegExpLiteral: {
        auto* re = node->Cast<RegExpLiteral>();
        const bool has_u = re->regexp_flags.find('u') != std::string_view::npos;
        const bool has_v = re->regexp_flags.find('v') != std::string_view::npos;
        const bool unicode = has_u || has_v;
        if (r.Chance(kOperationRegExpFlagChance())) {
          if (r.Index(3) != 0) {
            // Toggle one syntax-neutral flag without changing the pattern.
            const char flag = kSyntaxNeutralRegExpFlags[r.Index(
                kSyntaxNeutralRegExpFlags.size())];
            std::string flags(re->regexp_flags);
            const size_t at = flags.find(flag);
            if (at == std::string::npos) {
              flags.push_back(flag);
            } else {
              flags.erase(at, 1);
            }
            ctx.detail = "regexp flags /" + std::string(re->regexp_flags) +
                         " -> /" + flags;
            re->regexp_flags = t->Intern(flags);
            return true;
          }
          // Switch unicode mode (`none`, `u`, or `v`), never setting both `u`
          // and `v` together.
          const int cur_mode = has_v ? 2 : (has_u ? 1 : 0);
          const int target_mode =
              (cur_mode + 1 + static_cast<int>(r.Coin())) % 3;
          std::string flags;
          flags.reserve(re->regexp_flags.size() + 1);
          for (char c : re->regexp_flags) {
            if (c != 'u' && c != 'v') {
              flags.push_back(c);
            }
          }
          if (target_mode == 1) {
            flags.push_back('u');
          } else if (target_mode == 2) {
            flags.push_back('v');
          }
          std::string new_pattern(re->pattern);
          if (!IsModeNeutralRegExpPattern(re->pattern)) {
            new_pattern = CodeGen::RandomRegExpPattern(r, target_mode != 0,
                                                       target_mode == 2);
          }
          ctx.detail =
              "regexp mode /" + std::string(re->regexp_flags) + " -> /" + flags;
          re->pattern = t->Intern(new_pattern);
          re->regexp_flags = t->Intern(flags);
          return true;
        }
        if (r.Chance(kOperationRegExpPieceEditChance())) {
          std::string new_pattern;
          std::string what;
          if (MutateRegExpPatternInPlace(r, re->pattern, unicode, has_v,
                                         &new_pattern, &what) &&
              !new_pattern.empty() && new_pattern != re->pattern) {
            re->pattern = t->Intern(new_pattern);
            ctx.detail = "regexp pattern: " + what;
            return true;
          }
        }
        if (r.Coin()) {
          std::string new_pattern =
              CodeGen::RandomRegExpPattern(r, unicode, has_v);
          if (new_pattern.empty() || new_pattern == re->pattern) {
            return false;
          }
          re->pattern = t->Intern(new_pattern);
          ctx.detail = "regexp pattern replaced";
          return true;
        }
        std::string new_flags = CodeGen::RandomRegExpFlags(r);
        const bool new_v = new_flags.find('v') != std::string::npos;
        const bool new_u = new_v || new_flags.find('u') != std::string::npos;
        std::string new_pattern = CodeGen::RandomRegExpPattern(r, new_u, new_v);
        if (new_pattern.empty() ||
            (new_pattern == re->pattern && new_flags == re->regexp_flags)) {
          return false;
        }
        re->pattern = t->Intern(new_pattern);
        re->regexp_flags = t->Intern(new_flags);
        ctx.detail = "regexp replaced /" + new_flags;
        return true;
      }
      case NodeKind::kIfStatement: {
        // Inverts the condition (Fuzzilli: BeginIf.inverted), or removes an
        // inversion.
        auto* s = node->Cast<IfStatement>();
        if (s->test->Is<UnaryExpression>() &&
            s->test->Cast<UnaryExpression>()->op == UnaryOp::kNot) {
          s->test = s->test->Cast<UnaryExpression>()->argument;
          ctx.detail = "if condition: inversion removed";
        } else {
          s->test = mk::Unary(t, UnaryOp::kNot, s->test);
          ctx.detail = "if condition inverted";
        }
        b.Invalidate();
        return true;
      }
      default:
        return false;
    }
  }
};

// --- Input mutator
// -------------------------------------------------------------------

REDLINE_TUNABLE(kInputSkipUndefinedChance,
                "mutators.Input.skip_undefined_chance", 0.7, 0, 1,
                "probability that a reference to `undefined` is skipped as an "
                "Input candidate");
REDLINE_TUNABLE(
    kInputMaxAttempts, "mutators.Input.max_attempts", 8, 1, 16,
    "identifier references the Input mutator tries before giving up");
REDLINE_TUNABLE(kInputTypeMismatchChance, "mutators.Input.type_mismatch_chance",
                0.2, 0, 1,
                "probability that a visible variable of a different type is an "
                "acceptable replacement");
REDLINE_TUNABLE(kInputLiteralChance, "mutators.Input.literal_chance", 0.15, 0,
                1,
                "probability that a value reference is replaced by a literal "
                "rather than another variable");
REDLINE_TUNABLE(kInputSubtypeChance, "mutators.Input.subtype_chance", 0.5, 0, 1,
                "probability that a visible variable of an object subtype (an "
                "array, a map, a function and others) is an acceptable "
                "replacement for an object");

// Types whose values are objects with a more specific behavior. A variable
// of such a type can replace a plain object. Fuzzilli's InputMutator
// (aware) uses subtypes the same way.
bool IsObjectSubtype(JsType type) {
  switch (type) {
    case JsType::kArray:
    case JsType::kFunction:
    case JsType::kClass:
    case JsType::kRegExp:
    case JsType::kMap:
    case JsType::kSet:
    case JsType::kPromise:
    case JsType::kTypedArray:
    case JsType::kArrayBuffer:
    case JsType::kDate:
    case JsType::kError:
    case JsType::kDataView:
    case JsType::kSharedArrayBuffer:
    case JsType::kWeakMap:
    case JsType::kWeakSet:
    case JsType::kWeakRef:
    case JsType::kFinalizationRegistry:
    case JsType::kIterator:
    case JsType::kDisposableStack:
    case JsType::kAsyncDisposableStack:
    case JsType::kTemporalInstant:
    case JsType::kTemporalDuration:
    case JsType::kTemporalPlainDate:
    case JsType::kTemporalPlainTime:
    case JsType::kTemporalPlainDateTime:
    case JsType::kTemporalPlainYearMonth:
    case JsType::kTemporalPlainMonthDay:
    case JsType::kTemporalZonedDateTime:
    case JsType::kIntlLocale:
      return true;
    default:
      return false;
  }
}

class InputMutator final : public Mutator {
 public:
  // Fuzzilli: InputMutator (loose 2 + aware 1), up to 7 simultaneous.
  InputMutator()
      : Mutator("Input",
                "replaces a variable reference by another visible variable", 3,
                false, 7) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    ScopeAnalysis& scopes = b.scopes();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      if (!n->Is<Identifier>()) {
        return false;
      }
      auto* id = n->Cast<Identifier>();
      if (!scopes.IsReference(id)) {
        return false;
      }
      if (id->name == "undefined" && r.Chance(kInputSkipUndefinedChance())) {
        return false;
      }
      return b.RoleOf(n) != ExpressionRole::kNotReplaceable;
    });
    for (int attempt = 0;
         attempt < kInputMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      auto* id = candidates[r.Index(candidates.size())]->Cast<Identifier>();
      ExpressionRole role = b.RoleOf(id);
      std::vector<Variable> visible = b.VisibleAt(id);
      Binding* binding = scopes.BindingOf(id);
      JsType type =
          binding != nullptr ? b.GuessType(binding) : b.GuessExpressionType(id);
      std::vector<const Variable*> options;
      for (const Variable& v : visible) {
        if (v.name == id->name) {
          continue;
        }
        if (v.name == "arguments" && !b.ContextAt(id).has_arguments) {
          continue;
        }
        if (role == ExpressionRole::kTarget &&
            (v.is_constant || v.name == "arguments")) {
          continue;
        }
        if (type == JsType::kUnknown || v.type == type ||
            v.type == JsType::kUnknown ||
            (type == JsType::kObject && IsObjectSubtype(v.type) &&
             r.Chance(kInputSubtypeChance())) ||
            r.Chance(kInputTypeMismatchChance())) {
          options.push_back(&v);
        }
      }
      if (role == ExpressionRole::kValue && r.Chance(kInputLiteralChance())) {
        // Replace by a literal of the same type instead.
        GenContext gen = ContextForNode(b, id, ctx);
        CodeGen cg(gen);
        std::string old(id->name);
        if (ReplaceAnchored(b, id, cg.Literal(type)) != nullptr) {
          ctx.detail = "input " + old + " -> literal";
          return true;
        }
        continue;
      }
      if (options.empty()) {
        continue;
      }
      const Variable& choice = *options[r.Index(options.size())];
      // Don't create `delete name` in strict code.
      Node* parent = b.ParentOf(id);
      if (parent != nullptr && parent->Is<UnaryExpression>() &&
          parent->Cast<UnaryExpression>()->op == UnaryOp::kDelete) {
        continue;
      }
      ctx.detail =
          "input " + std::string(id->name) + " -> " + std::string(choice.name);
      id->name = b.tree()->Intern(choice.name);
      b.Invalidate();
      return true;
    }
    return false;
  }
};

// --- Remove mutator
// -----------------------------------------------------------------

REDLINE_TUNABLE(kRemoveElementChance, "mutators.Remove.element_chance", 0.3, 0,
                1,
                "probability of removing an argument, element or property "
                "instead of a statement");
REDLINE_TUNABLE(kRemoveMaxAttempts, "mutators.Remove.max_attempts", 8, 1, 16,
                "statement slots the Remove mutator tries before giving up");

class RemoveMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default (enable with
  // --mutator-weights).
  RemoveMutator()
      : Mutator("Remove", "removes a statement, argument, element or property",
                0, false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    if (r.Chance(kRemoveElementChance()) && RemoveElement(b, ctx)) {
      return true;
    }
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0;
         attempt < kRemoveMaxAttempts.AsInt() && !slots.empty(); attempt++) {
      const Builder::Slot& slot = slots[r.Index(slots.size())];
      Statement* s = slot.statement();
      if (s == nullptr || IsDirective(s)) {
        continue;
      }
      // Removing a declaration that later code still uses only produces a
      // ReferenceError: never do that.
      if (!b.EscapingBindings({s}, /*block_scoped_only=*/false).empty()) {
        continue;
      }
      ctx.detail =
          std::string("removed ") + s->kind_name() + " `" + Describe(s) + "`";
      b.Remove(slot);
      return true;
    }
    return false;
  }

 private:
  bool RemoveElement(Builder& b, MutationContext& ctx) {
    Random& r = b.rng();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      switch (n->kind) {
        case NodeKind::kCallExpression:
          return !n->Cast<CallExpression>()->arguments.empty();
        case NodeKind::kNewExpression:
          return !n->Cast<NewExpression>()->arguments.empty();
        case NodeKind::kArrayExpression:
          return !n->Cast<ArrayExpression>()->elements.empty();
        case NodeKind::kObjectExpression:
          return !n->Cast<ObjectExpression>()->properties.empty();
        default:
          return false;
      }
    });
    if (candidates.empty()) {
      return false;
    }
    Node* node = candidates[r.Index(candidates.size())];
    switch (node->kind) {
      case NodeKind::kCallExpression: {
        auto& args = node->Cast<CallExpression>()->arguments;
        args.erase(args.begin() +
                   static_cast<std::ptrdiff_t>(r.Index(args.size())));
        ctx.detail = "removed call argument";
        break;
      }
      case NodeKind::kNewExpression: {
        auto& args = node->Cast<NewExpression>()->arguments;
        args.erase(args.begin() +
                   static_cast<std::ptrdiff_t>(r.Index(args.size())));
        ctx.detail = "removed constructor argument";
        break;
      }
      case NodeKind::kArrayExpression: {
        auto& elements = node->Cast<ArrayExpression>()->elements;
        elements.erase(elements.begin() +
                       static_cast<std::ptrdiff_t>(r.Index(elements.size())));
        ctx.detail = "removed array element";
        break;
      }
      case NodeKind::kObjectExpression: {
        auto& props = node->Cast<ObjectExpression>()->properties;
        props.erase(props.begin() +
                    static_cast<std::ptrdiff_t>(r.Index(props.size())));
        ctx.detail = "removed object property";
        break;
      }
      default:
        return false;
    }
    b.Invalidate();
    return true;
  }
};

// --- Insert (code generation) mutator
// --------------------------------------------------

REDLINE_TUNABLE(kInsertSlotAttempts, "mutators.Insert.slot_attempts", 6, 1, 16,
                "slots drawn to find one with a function in scope when "
                "preferring functions");
REDLINE_TUNABLE(kInsertExtraContinueChance,
                "mutators.Insert.extra_continue_chance", 0.6, 0, 1,
                "probability of inserting one more generator output beyond the "
                "minimum (geometric)");
REDLINE_TUNABLE(kInsertMemberSlotWeight, "mutators.Insert.member_slot_weight",
                1, 0, 100,
                "weight of a member slot (class body or object literal) "
                "against a statement slot when CodeGen picks where to insert");

class InsertMutator final : public Mutator {
 public:
  // Inserts between min_count and min_count + extra_count generator outputs
  // (geometrically distributed) at a random slot, or at the top of the
  // program with `at_top` (the "fresh values" prefix every mutation chain
  // starts with, after Fuzzilli's buildPrefix()). `max_simultaneous` slots
  // may be filled per application (Fuzzilli's CodeGenMutator: 3).
  InsertMutator(std::string name, std::string_view description, double weight,
                std::vector<std::string> generator_names, bool prefer_functions,
                int min_count = 1, int extra_count = 2, bool at_top = false,
                int max_simultaneous = 1, bool cannot_throw = false)
      : Mutator(std::move(name), description, weight, false, max_simultaneous),
        generator_names_(std::move(generator_names)),
        prefer_functions_(prefer_functions),
        min_count_(min_count),
        extra_count_(extra_count),
        at_top_(at_top),
        cannot_throw_(cannot_throw) {}

  // How many generator outputs one application inserts: min_count plus a
  // geometrically distributed 0..extra_count.
  bool SetInsertCounts(int min_count, int extra_count) override {
    min_count_ = min_count;
    extra_count_ = extra_count;
    return true;
  }

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    Builder::Slot slot;
    // Like Fuzzilli's CodeGenMutator, CodeGen also inserts into class bodies
    // and object literals: member slots compete with statement slots.
    if (!at_top_ && generator_names_.empty() && kInsertMemberSlotWeight() > 0) {
      std::vector<Builder::MemberSlot> member_slots = b.MemberSlots();
      if (!member_slots.empty()) {
        const double members = kInsertMemberSlotWeight() *
                               static_cast<double>(member_slots.size());
        const double statements =
            static_cast<double>(b.InsertionSlots().size());
        if (r.Chance(members / (members + statements))) {
          return InsertMembers(b, ctx, r.Pick(member_slots));
        }
      }
    }
    if (at_top_) {
      std::vector<Builder::Slot> slots = b.InsertionSlots();
      for (const Builder::Slot& s : slots) {
        if (s.owner == b.root()) {
          slot = s;
          break;
        }
      }
      if (!slot.valid()) {
        return false;
      }
    }
    for (int attempt = 0; attempt < kInsertSlotAttempts.AsInt() && !at_top_;
         attempt++) {
      slot = b.RandomInsertionSlot();
      if (!slot.valid()) {
        return false;
      }
      if (!prefer_functions_) {
        break;
      }
      std::vector<Variable> visible = b.VisibleAtSlot(slot);
      if (!Builder::OfType(visible, JsType::kFunction).empty()) {
        break;
      }
    }
    GenContext gen = ContextForSlot(b, slot, ctx);
    gen.cannot_throw = cannot_throw_;
    std::vector<Statement*> out;
    gen.out = &out;
    int count =
        min_count_ + r.SmallCount(extra_count_, kInsertExtraContinueChance());
    std::string names;
    for (int i = 0; i < count; i++) {
      Generator* g = PickGenerator(gen);
      if (g == nullptr) {
        break;
      }
      g->stats().invocations++;
      size_t before = out.size();
      if (g->Generate(gen, &out) && out.size() > before) {
        g->stats().produced++;
        NoteGeneratorUsed(g);
        if (!names.empty()) {
          names += '+';
        }
        names += g->name();
      } else {
        out.resize(before);
      }
    }
    if (out.empty()) {
      return false;
    }
    b.InsertAll(slot, out);
    ctx.detail = "inserted " + names;
    return true;
  }

 private:
  // Inserts the output of one member generator at `slot`.
  bool InsertMembers(Builder& b, MutationContext& ctx,
                     const Builder::MemberSlot& slot) {
    GenContext gen = ContextForNode(b, slot.owner, ctx);
    std::vector<Node*> members;
    std::vector<Statement*> unused;
    gen.member_owner = slot.owner;
    gen.member_out = &members;
    Generator* g = PickGenerator(gen);
    if (g == nullptr) {
      return false;
    }
    g->stats().invocations++;
    if (!g->Generate(gen, &unused) || members.empty()) {
      return false;
    }
    g->stats().produced++;
    NoteGeneratorUsed(g);
    Builder::MemberSlot cursor = slot;
    for (Node* member : members) {
      b.InsertMember(cursor, member);
      cursor.index++;
    }
    ctx.detail = "inserted member " + g->name();
    return true;
  }

  // Asks the generator strategy for one generator that is applicable here
  // and, if this mutator has a generator list, on that list.
  Generator* PickGenerator(const GenContext& gen) {
    const GeneratorSet& set = gen.generators;
    auto eligible = [&](const Generator& g) {
      if (!generator_names_.empty() &&
          std::find(generator_names_.begin(), generator_names_.end(),
                    g.name()) == generator_names_.end()) {
        return false;
      }
      return g.Applicable(gen);
    };
    size_t index = PickWithMask<Generator>(set.selector(), set.candidates(),
                                           gen.builder.rng(), eligible,
                                           ActiveGeneratorMask());
    return index == Selector<Generator>::kNoChoice ? nullptr
                                                   : set.candidates()[index];
  }

  std::vector<std::string> generator_names_;
  bool prefer_functions_;
  int min_count_;
  int extra_count_;
  bool at_top_;
  bool cannot_throw_;
};

// --- Expression mutator
// -------------------------------------------------------------

REDLINE_TUNABLE(
    kExpressionMaxAttempts, "mutators.Expression.max_attempts", 8, 1, 16,
    "value expressions the Expression mutator tries before giving up");
REDLINE_TUNABLE(kExpressionComplexCandidateChance,
                "mutators.Expression.complex_candidate_chance", 0.2, 0, 1,
                "probability that a spread, template, function or class "
                "expression is kept as a candidate");
REDLINE_TUNABLE(kPropertyAccessorBothChance,
                "mutators.PropertyAccessor.both_chance", 0.5, 0, 1,
                "probability that an Object.defineProperty accessor defines "
                "both get and set");
REDLINE_TUNABLE(kPropertyAccessorGetterChance,
                "mutators.PropertyAccessor.getter_chance", 0.75, 0, 1,
                "probability that a single accessor is a getter rather than a "
                "setter");
REDLINE_TUNABLE(kPropertyAccessorMaxStatements,
                "mutators.PropertyAccessor.max_statements", 2, 0, 16,
                "most generated statements in an accessor body before "
                "returning");
REDLINE_TUNABLE(kPropertyAccessorMaxAttempts,
                "mutators.PropertyAccessor.max_attempts", 8, 1, 16,
                "candidates the PropertyAccessor mutator tries before giving "
                "up");

bool CanMoveIntoAccessor(Expression* expr) {
  if (expr == nullptr) {
    return true;
  }
  Requirements req = AnalyzeRequirements(expr);
  return !req.needs_function && !req.needs_async && !req.needs_generator &&
         !req.needs_break && !req.needs_continue && !req.needs_arguments &&
         !req.needs_super && !req.needs_new_target && !req.has_external_label;
}

bool MethodCanBecomeReturnedFunction(FunctionExpression* fn) {
  if (fn == nullptr || fn->body == nullptr) {
    return false;
  }
  if (AnalyzeRequirements(fn->body).needs_super) {
    return false;
  }
  for (Expression* param : fn->params) {
    if (param != nullptr && AnalyzeRequirements(param).needs_super) {
      return false;
    }
  }
  return true;
}

bool IsForbiddenObjectAccessorKey(Expression* key) {
  if (key == nullptr || key->Is<PrivateIdentifier>()) {
    return true;
  }
  if (auto* id = key->As<Identifier>()) {
    return id->name == "__proto__";
  }
  if (auto* s = key->As<StringLiteral>()) {
    return s->value == "__proto__";
  }
  return false;
}

bool IsForbiddenClassAccessorKey(Expression* key) {
  if (key == nullptr || key->Is<PrivateIdentifier>()) {
    return true;
  }
  if (auto* id = key->As<Identifier>()) {
    return id->name == "constructor" || id->name == "prototype" ||
           id->name == "__proto__";
  }
  if (auto* s = key->As<StringLiteral>()) {
    return s->value == "constructor" || s->value == "prototype" ||
           s->value == "__proto__";
  }
  return !key->Is<NumericLiteral>();
}

bool IsInPatternOrAssignmentTarget(Builder& b, Node* node) {
  for (Node *p = node, *up = b.ParentOf(p); up != nullptr;
       p = up, up = b.ParentOf(p)) {
    if (up->Is<AssignmentPattern>()) {
      if (up->Cast<AssignmentPattern>()->right == p) {
        break;
      }
      return true;
    }
    if (up->IsPattern()) {
      return true;
    }
    if (up->Is<AssignmentExpression>() &&
        up->Cast<AssignmentExpression>()->left == p) {
      return true;
    }
    if (up->Is<ForInStatement>() && up->Cast<ForInStatement>()->left == p) {
      return true;
    }
    if (up->Is<ForOfStatement>() && up->Cast<ForOfStatement>()->left == p) {
      return true;
    }
  }
  return false;
}

class ExpressionMutator final : public Mutator {
 public:
  // No direct Fuzzilli counterpart. The closest are the expression edits of
  // OperationMutator. Up to 3 simultaneous.
  ExpressionMutator()
      : Mutator("Expression",
                "replaces or wraps an expression in value position", 0.5, false,
                3) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Expression*> candidates = b.ValueExpressions();
    for (int attempt = 0;
         attempt < kExpressionMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      Expression* e = candidates[r.Index(candidates.size())];
      if ((e->Is<SpreadElement>() || e->Is<TemplateLiteral>() ||
           e->Is<FunctionExpression>() || e->Is<ClassExpression>() ||
           e->Is<ArrowFunctionExpression>()) &&
          !r.Chance(kExpressionComplexCandidateChance())) {
        continue;
      }
      Node* parent = b.ParentOf(e);
      if (parent == nullptr) {
        continue;
      }
      if (parent->Is<ExpressionStatement>() &&
          parent->Cast<ExpressionStatement>()->is_directive) {
        continue;
      }
      if (parent->Is<SpreadElement>() && !r.Coin()) {
        continue;
      }
      bool under_delete =
          parent->Is<UnaryExpression>() &&
          parent->Cast<UnaryExpression>()->op == UnaryOp::kDelete;
      bool exp_left = parent->Is<BinaryExpression>() &&
                      parent->Cast<BinaryExpression>()->op == BinaryOp::kExp &&
                      parent->Cast<BinaryExpression>()->left == e;
      GenContext gen = ContextForNode(b, e, ctx);
      CodeGen cg(gen);
      JsType type = b.GuessExpressionType(e);
      Expression* replacement = nullptr;
      std::string how;
      switch (r.Index(10)) {
        case 0:
        case 1:
        case 2: {
          Identifier* v = cg.Variable(type);
          if (v == nullptr || under_delete) {
            continue;
          }
          replacement = v;
          how = "variable";
          break;
        }
        case 3:
        case 4:
          replacement = cg.Literal(type);
          how = "literal";
          break;
        case 5:
        case 6:
          replacement = cg.Expression(1);
          how = "expression";
          break;
        default: {
          if (e->Is<SpreadElement>()) {
            continue;  // `...x + 1` is not an expression
          }
          replacement = Wrap(cg, e, gen, exp_left);
          how = "wrap";
          break;
        }
      }
      if (replacement == nullptr) {
        continue;
      }
      if (under_delete && replacement->Is<Identifier>()) {
        continue;
      }
      std::string old = Describe(e);
      if (ReplaceAnchored(b, e, replacement) == nullptr) {
        continue;
      }
      ctx.detail = "expression `";
      ctx.detail += old;
      ctx.detail += "` -> ";
      ctx.detail += how;
      return true;
    }
    return false;
  }

 private:
  // Wraps `e` in a new expression that uses it. `e` is detached from its
  // parent by the caller's Replace; it may be reused inside the wrapper.
  Expression* Wrap(CodeGen& cg, Expression* e, GenContext& gen, bool exp_left) {
    Random& r = cg.rng();
    Tree* t = cg.tree();
    switch (r.Index(14)) {
      case 0:
        return mk::Binary(t, CodeGen::RandomArithmeticOp(r), e,
                          cg.Value(JsType::kNumber, 1));
      case 1:
        return mk::Binary(t, CodeGen::RandomArithmeticOp(r),
                          cg.Value(JsType::kUnknown, 1), e);
      case 2:
        return mk::Call(t,
                        mk::Id(t, r.Pick({"String", "Number", "Boolean",
                                          "Object", "BigInt", "Symbol"})),
                        {e});
      case 3:
        return mk::CallMethod(t, mk::Id(t, "Math"),
                              r.Pick({"abs", "floor", "round", "sqrt", "trunc",
                                      "fround", "sign"}),
                              {e});
      case 4:
        return mk::Array(t, {e});
      case 5:
        return mk::Object(t, {mk::Prop(t, "x", e)});
      case 6:
        if (exp_left) {
          return nullptr;
        }
        return mk::Unary(t, CodeGen::RandomUnaryOp(r), e);
      case 7:
        return mk::Cond(t, cg.Condition(1), e, cg.Value(JsType::kUnknown, 1));
      case 8:
        return mk::Cond(t, cg.Condition(1), cg.Value(JsType::kUnknown, 1), e);
      case 9:
        return mk::Seq(t, {cg.Value(JsType::kUnknown, 1), e});
      case 10:
        if (gen.syntax.in_async && !gen.syntax.in_static_block && !exp_left) {
          return mk::Await(t, e);
        }
        if (!CanMoveIntoAccessor(e)) {
          return nullptr;
        }
        return mk::Call(t, mk::ArrowExpr(t, {}, e), {});
      case 11:
        return mk::Logical(t, CodeGen::RandomLogicalOp(r), e,
                           cg.Value(JsType::kUnknown, 1));
      case 12:
        return mk::CallMethod(
            t, mk::Id(t, "JSON"), "parse",
            {mk::CallMethod(t, mk::Id(t, "JSON"), "stringify", {e})});
      default: {
        std::string_view p = gen.builder.FreshName("x");
        return mk::Call(t, mk::ArrowExpr(t, {p}, mk::Id(t, p)), {e});
      }
    }
  }
};

// --- PropertyAccessor mutator
// ---------------------------------------------------------

// Fuzzilli's PropertyAccessorMutator (weight 1, up to 3 simultaneous).
// Changes a property store, an object literal property, a class method or
// a class field into a getter, a setter or both.
class PropertyAccessorMutator final : public Mutator {
 public:
  PropertyAccessorMutator()
      : Mutator("PropertyAccessor",
                "turns a property, method or field into a getter or setter", 1,
                false, 3) {}

  static bool IsAccessorCandidate(Builder& b, Node* n) {
    if (IsInPatternOrAssignmentTarget(b, n)) {
      return false;
    }
    switch (n->kind) {
      case NodeKind::kAssignmentExpression: {
        auto* e = n->Cast<AssignmentExpression>();
        if (e->op != AssignOp::kAssign ||
            b.RoleOf(e) != ExpressionRole::kValue ||
            !e->left->Is<MemberExpression>()) {
          return false;
        }
        auto* m = e->left->Cast<MemberExpression>();
        if (m->optional || m->object->Is<Super>() ||
            m->property->Is<PrivateIdentifier>() ||
            (!m->computed && !m->property->Is<Identifier>())) {
          return false;
        }
        return CanMoveIntoAccessor(e->right);
      }
      case NodeKind::kProperty: {
        auto* prop = n->Cast<Property>();
        Node* parent = b.ParentOf(prop);
        if (parent == nullptr || !parent->Is<ObjectExpression>() ||
            b.RoleOf(parent) != ExpressionRole::kValue ||
            prop->property_kind != PropertyKind::kInit ||
            IsForbiddenObjectAccessorKey(prop->key)) {
          return false;
        }
        if (prop->method) {
          return prop->value->Is<FunctionExpression>() &&
                 MethodCanBecomeReturnedFunction(
                     prop->value->Cast<FunctionExpression>());
        }
        return CanMoveIntoAccessor(prop->value);
      }
      case NodeKind::kMethodDefinition: {
        auto* m = n->Cast<MethodDefinition>();
        Node* parent = b.ParentOf(m);
        if (parent == nullptr || !parent->Is<ClassBody>() ||
            m->method_kind != MethodKind::kMethod || m->computed ||
            IsForbiddenClassAccessorKey(m->key)) {
          return false;
        }
        return MethodCanBecomeReturnedFunction(m->value);
      }
      case NodeKind::kPropertyDefinition: {
        auto* f = n->Cast<PropertyDefinition>();
        Node* parent = b.ParentOf(f);
        if (parent == nullptr || !parent->Is<ClassBody>() ||
            f->is_auto_accessor || f->computed ||
            IsForbiddenClassAccessorKey(f->key)) {
          return false;
        }
        return CanMoveIntoAccessor(f->value);
      }
      default:
        return false;
    }
  }

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Node*> candidates =
        b.Collect([&](Node* n) { return IsAccessorCandidate(b, n); });
    for (int attempt = 0;
         attempt < kPropertyAccessorMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      Node* node = candidates[r.Index(candidates.size())];
      if (ConvertToAccessor(b, node, ctx)) {
        return true;
      }
    }
    return false;
  }

  bool ConvertToAccessor(Builder& b, Node* node, MutationContext& ctx) {
    Random& r = b.rng();
    Tree* t = b.tree();
    GenContext gen = ContextForNode(b, node, ctx);
    const int max_stmts = kPropertyAccessorMaxStatements.AsInt();
    switch (node->kind) {
      case NodeKind::kAssignmentExpression: {
        auto* e = node->Cast<AssignmentExpression>();
        auto* m = e->left->Cast<MemberExpression>();
        Expression* key_expr =
            m->computed ? m->property
                        : mk::Str(t, m->property->Cast<Identifier>()->name);
        bool both = r.Chance(kPropertyAccessorBothChance());
        bool has_get = both || r.Chance(kPropertyAccessorGetterChance());
        bool has_set = both || !has_get;
        std::vector<Node*> desc_props;
        if (has_get) {
          GenContext g_ctx = gen.Nested(true, false, false, false);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          std::vector<Statement*> g_body = CodeGen(g_ctx).Body(g_ctx, n_stmts);
          g_body.push_back(mk::Return(t, e->right));
          desc_props.push_back(
              mk::Method(t, "get", mk::Function(t, "", {}, std::move(g_body))));
        }
        if (has_set) {
          std::string_view param = b.FreshName("v");
          GenContext s_ctx = gen.Nested(true, false, false, false);
          s_ctx.Declare(param, JsType::kUnknown);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          std::vector<Statement*> s_body = CodeGen(s_ctx).Body(s_ctx, n_stmts);
          if (!has_get) {
            s_body.push_back(mk::ExprStmt(t, e->right));
          }
          desc_props.push_back(mk::Method(
              t, "set", mk::Function(t, "", {param}, std::move(s_body))));
        }
        desc_props.push_back(mk::Prop(t, "configurable", mk::Bool(t, true)));
        desc_props.push_back(mk::Prop(t, "enumerable", mk::Bool(t, r.Coin())));
        Expression* replacement = mk::CallMethod(
            t, mk::Id(t, "Object"), "defineProperty",
            {m->object, key_expr, mk::Object(t, std::move(desc_props))});
        std::string old = Describe(e);
        if (ReplaceAnchored(b, e, replacement) == nullptr) {
          return false;
        }
        ctx.detail = "expression `" + old + "` -> accessor defineProperty";
        return true;
      }
      case NodeKind::kProperty: {
        auto* prop = node->Cast<Property>();
        std::string old = Describe(prop);
        if (prop->method) {
          auto* fn = prop->value->Cast<FunctionExpression>();
          fn->name = nullptr;
          GenContext g_ctx = gen.Nested(true, false, false, false);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          std::vector<Statement*> g_body = CodeGen(g_ctx).Body(g_ctx, n_stmts);
          g_body.push_back(mk::Return(t, fn));
          FunctionExpression* getter =
              mk::Function(t, "", {}, std::move(g_body));
          if (prop->start >= 0) {
            mk::Anchor(getter, prop->start);
          }
          prop->property_kind = PropertyKind::kGet;
          prop->method = false;
          prop->shorthand = false;
          prop->value = getter;
          b.Invalidate();
          ctx.detail = "property `" + old + "` -> getter";
          return true;
        }
        if (prop->key == prop->value && prop->key->Is<Identifier>()) {
          prop->key = mk::Id(t, prop->key->Cast<Identifier>()->name);
        }
        Expression* old_val = prop->value;
        bool is_get = r.Chance(kPropertyAccessorGetterChance());
        FunctionExpression* acc_fn = nullptr;
        if (is_get) {
          GenContext g_ctx = gen.Nested(true, false, false, false);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          std::vector<Statement*> g_body = CodeGen(g_ctx).Body(g_ctx, n_stmts);
          g_body.push_back(mk::Return(t, old_val));
          acc_fn = mk::Function(t, "", {}, std::move(g_body));
          prop->property_kind = PropertyKind::kGet;
        } else {
          std::string_view param = b.FreshName("v");
          GenContext s_ctx = gen.Nested(true, false, false, false);
          s_ctx.Declare(param, JsType::kUnknown);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          std::vector<Statement*> s_body = CodeGen(s_ctx).Body(s_ctx, n_stmts);
          if (old_val != nullptr) {
            s_body.push_back(mk::ExprStmt(t, old_val));
          }
          acc_fn = mk::Function(t, "", {param}, std::move(s_body));
          prop->property_kind = PropertyKind::kSet;
        }
        if (prop->start >= 0) {
          mk::Anchor(acc_fn, prop->start);
        }
        prop->shorthand = false;
        prop->method = false;
        prop->value = acc_fn;
        b.Invalidate();
        ctx.detail =
            "property `" + old + (is_get ? "` -> getter" : "` -> setter");
        return true;
      }
      case NodeKind::kMethodDefinition: {
        auto* m = node->Cast<MethodDefinition>();
        std::string old = Describe(m);
        FunctionExpression* fn = m->value;
        fn->name = nullptr;
        GenContext g_ctx = gen.Nested(true, false, false, false);
        int n_stmts = static_cast<int>(r.Range(0, max_stmts));
        std::vector<Statement*> g_body = CodeGen(g_ctx).Body(g_ctx, n_stmts);
        g_body.push_back(mk::Return(t, fn));
        FunctionExpression* getter = mk::Function(t, "", {}, std::move(g_body));
        if (m->start >= 0) {
          mk::Anchor(getter, m->start);
        }
        m->method_kind = MethodKind::kGet;
        m->value = getter;
        b.Invalidate();
        ctx.detail = "class method `" + old + "` -> getter";
        return true;
      }
      case NodeKind::kPropertyDefinition: {
        auto* f = node->Cast<PropertyDefinition>();
        std::string old = Describe(f);
        bool is_get = r.Chance(kPropertyAccessorGetterChance());
        FunctionExpression* acc_fn = nullptr;
        if (is_get) {
          GenContext g_ctx = gen.Nested(true, false, false, false);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          CodeGen cg(g_ctx);
          std::vector<Statement*> g_body = cg.Body(g_ctx, n_stmts);
          Expression* ret_val =
              f->value != nullptr ? f->value : cg.Value(JsType::kUnknown, 1);
          g_body.push_back(mk::Return(t, ret_val));
          acc_fn = mk::Function(t, "", {}, std::move(g_body));
        } else {
          std::string_view param = b.FreshName("v");
          GenContext s_ctx = gen.Nested(true, false, false, false);
          s_ctx.Declare(param, JsType::kUnknown);
          int n_stmts = static_cast<int>(r.Range(0, max_stmts));
          std::vector<Statement*> s_body = CodeGen(s_ctx).Body(s_ctx, n_stmts);
          if (f->value != nullptr) {
            s_body.push_back(mk::ExprStmt(t, f->value));
          }
          acc_fn = mk::Function(t, "", {param}, std::move(s_body));
        }
        MethodDefinition* m = mk::ClassMethod(
            t, f->key, acc_fn, is_get ? MethodKind::kGet : MethodKind::kSet,
            f->is_static);
        m->computed = f->computed;
        if (ReplaceAnchored(b, f, m) == nullptr) {
          return false;
        }
        ctx.detail =
            "class field `" + old + (is_get ? "` -> getter" : "` -> setter");
        return true;
      }
      default:
        return false;
    }
  }
};

// --- Wrap mutator
// ---------------------------------------------------------------------

REDLINE_TUNABLE(kWrapMaxAttempts, "mutators.Wrap.max_attempts", 8, 1, 16,
                "statement slots the Wrap mutator tries before giving up");
REDLINE_TUNABLE(kWrapMaxExtraStatements, "mutators.Wrap.max_extra_statements",
                3, 0, 64,
                "most statements after the first that one wrapper encloses");
REDLINE_TUNABLE(
    kWrapExtraStatementChance, "mutators.Wrap.extra_statement_chance", 0.4, 0,
    1, "probability of extending the wrapped run by one more statement");

class WrapMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default.
  WrapMutator()
      : Mutator("Wrap", "wraps statements in a loop, if, try, label or IIFE", 0,
                false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0; attempt < kWrapMaxAttempts.AsInt() && !slots.empty();
         attempt++) {
      Builder::Slot slot = slots[r.Index(slots.size())];
      Statement* first = slot.statement();
      if (first == nullptr || IsDirective(first)) {
        continue;
      }
      // A run of 1..4 statements.
      size_t count =
          1 + static_cast<size_t>(r.SmallCount(kWrapMaxExtraStatements.AsInt(),
                                               kWrapExtraStatementChance()));
      count = std::min(count, slot.list->size() - slot.index);
      std::vector<Statement*> run;
      Requirements req;
      for (size_t i = 0; i < count; i++) {
        Statement* s = (*slot.list)[slot.index + i];
        if (IsDirective(s)) {
          break;
        }
        Requirements sr = AnalyzeRequirements(s);
        req.needs_function |= sr.needs_function;
        req.needs_async |= sr.needs_async;
        req.needs_generator |= sr.needs_generator;
        req.needs_break |= sr.needs_break;
        req.needs_continue |= sr.needs_continue;
        req.needs_sloppy |= sr.needs_sloppy;
        req.needs_arguments |= sr.needs_arguments;
        req.needs_super |= sr.needs_super;
        req.needs_new_target |= sr.needs_new_target;
        req.has_external_label |= sr.has_external_label;
        run.push_back(s);
      }
      if (run.empty()) {
        continue;
      }
      // Moving block-scoped declarations into a block hides them from the
      // code that uses them; `var`s survive every wrapper except the IIFE.
      std::vector<Node*> run_nodes(run.begin(), run.end());
      if (!b.EscapingBindings(run_nodes, /*block_scoped_only=*/true).empty()) {
        continue;
      }
      bool vars_escape =
          !b.EscapingBindings(run_nodes, /*block_scoped_only=*/false).empty();
      Tree* t = b.tree();
      GenContext gen = ContextForSlot(b, slot, ctx);
      CodeGen cg(gen);
      Statement* wrapper = nullptr;
      std::string how;
      switch (r.Index(8)) {
        case 0:
        case 1: {
          std::string_view counter = b.FreshName("i");
          static const double kCounts[] = {2, 3, 5, 10, 100, 1000, 10000};
          wrapper = mk::CountedLoop(t, counter, mk::Num(t, kCounts[r.Index(7)]),
                                    mk::Block(t, run));
          how = "loop";
          break;
        }
        case 2:
          wrapper = mk::If(t, cg.Condition(1), mk::Block(t, run));
          how = "if";
          break;
        case 3:
          wrapper = mk::Try(t, run, b.FreshName("e"), {});
          how = "try";
          break;
        case 4: {
          std::vector<Statement*> finalizer;
          wrapper = mk::Try(t, run, {}, {}, &finalizer);
          how = "try-finally";
          break;
        }
        case 5: {
          std::string_view label = b.FreshName("L");
          wrapper = mk::Labeled(t, label, mk::Block(t, run));
          how = "label";
          break;
        }
        case 6: {
          if (req.needs_generator || req.needs_break || req.needs_continue ||
              req.has_external_label || vars_escape) {
            continue;
          }
          Expression* fn = mk::Arrow(t, {}, run, req.needs_async);
          wrapper = mk::ExprStmt(t, mk::Call(t, fn, {}));
          how = "iife";
          break;
        }
        default: {
          std::string_view e = b.FreshName("e");
          wrapper = mk::ForOf(t, VariableKind::kConst, e, cg.ArrayLiteral(1),
                              mk::Block(t, run));
          how = "for-of";
          break;
        }
      }
      // Replace the run by the wrapper.
      slot.list->erase(
          slot.list->begin() + static_cast<std::ptrdiff_t>(slot.index),
          slot.list->begin() +
              static_cast<std::ptrdiff_t>(slot.index + run.size()));
      b.Invalidate();
      b.Insert(slot, wrapper);
      ctx.detail =
          "wrapped " + std::to_string(run.size()) + " statement(s) in " + how;
      return true;
    }
    return false;
  }
};

// --- Duplicate / Reorder
// ------------------------------------------------------------------

REDLINE_TUNABLE(kDuplicateMaxAttempts, "mutators.Duplicate.max_attempts", 8, 1,
                16,
                "statement slots the Duplicate mutator tries before giving up");

class DuplicateMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default.
  DuplicateMutator()
      : Mutator("Duplicate", "duplicates a statement", 0, false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0;
         attempt < kDuplicateMaxAttempts.AsInt() && !slots.empty(); attempt++) {
      Builder::Slot slot = slots[r.Index(slots.size())];
      Statement* s = slot.statement();
      if (s == nullptr || IsDirective(s) || IsLexicalDeclaration(s)) {
        continue;
      }
      if (s->Is<FunctionDeclaration>()) {
        // Duplicate function declarations are only fine in sloppy
        // function/script scope.
        Builder::Context c = b.ContextAtSlot(slot);
        bool function_body = slot.owner->Is<BlockStatement>() &&
                             b.ParentOf(slot.owner) != nullptr &&
                             b.ParentOf(slot.owner)->IsFunction();
        if (c.strict || !(slot.owner->Is<fast::Program>() || function_body)) {
          continue;
        }
      }
      Statement* copy = b.tree()->Clone(s);
      Builder::Slot where = slot;
      where.index = r.Coin() ? slot.index + 1
                             : static_cast<size_t>(r.Range(
                                   static_cast<int64_t>(slot.index) + 1,
                                   static_cast<int64_t>(slot.list->size())));
      b.Insert(where, copy);
      ctx.detail = std::string("duplicated ") + s->kind_name() + " `" +
                   Describe(s) + "`";
      return true;
    }
    return false;
  }
};

REDLINE_TUNABLE(kReorderMaxAttempts, "mutators.Reorder.max_attempts", 8, 1, 16,
                "statement slots the Reorder mutator tries before giving up");

class ReorderMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default.
  ReorderMutator()
      : Mutator("Reorder", "swaps two adjacent statements", 0, false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0;
         attempt < kReorderMaxAttempts.AsInt() && !slots.empty(); attempt++) {
      Builder::Slot slot = slots[r.Index(slots.size())];
      if (slot.index + 1 >= slot.list->size()) {
        continue;
      }
      Statement* a = (*slot.list)[slot.index];
      Statement* c = (*slot.list)[slot.index + 1];
      if (IsDirective(a) || IsDirective(c)) {
        continue;
      }
      // Do not move a declaration past a use of it (TDZ, undefined values).
      if (Uses(b, c, b.EscapingBindings({a}, false)) ||
          Uses(b, a, b.EscapingBindings({c}, false))) {
        continue;
      }
      std::swap((*slot.list)[slot.index], (*slot.list)[slot.index + 1]);
      b.Invalidate();
      ctx.detail =
          std::string("swapped ") + a->kind_name() + " and " + c->kind_name();
      return true;
    }
    return false;
  }
};

// --- Call mutator
// ------------------------------------------------------------------------

REDLINE_TUNABLE(kCallMaxAttempts, "mutators.Call.max_attempts", 8, 1, 16,
                "call/new expressions the Call mutator tries before giving up");
REDLINE_TUNABLE(kCallSpreadToggleChance, "mutators.Call.spread_toggle_chance",
                0.2, 0, 1,
                "probability that a Call mutation toggles spread on an "
                "argument");
REDLINE_TUNABLE(kCallOptionalCallChance, "mutators.Call.optional_call_chance",
                0.15, 0, 1,
                "probability that a Call mutation toggles optional chaining on "
                "a call expression");

class CallMutator final : public Mutator {
 public:
  // Closest Fuzzilli counterpart: PropertyAccessorMutator (weight 1, shared
  // with Expression), up to 3 simultaneous.
  CallMutator()
      : Mutator("Call",
                "adds/removes/spreads arguments, swaps callees, optional call "
                "or call <-> new",
                0.5, false, 3) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      if (auto* c = n->As<CallExpression>()) {
        return !c->callee->Is<Super>();
      }
      return n->Is<NewExpression>();
    });
    for (int attempt = 0;
         attempt < kCallMaxAttempts.AsInt() && !candidates.empty(); attempt++) {
      Node* node = candidates[r.Index(candidates.size())];
      Node* parent = b.ParentOf(node);
      if (parent == nullptr) {
        continue;
      }
      auto* call_expr = node->As<CallExpression>();
      Tree* t = b.tree();
      std::vector<Expression*>* args =
          call_expr != nullptr ? &call_expr->arguments
                               : &node->Cast<NewExpression>()->arguments;
      Expression*& callee = call_expr != nullptr
                                ? call_expr->callee
                                : node->Cast<NewExpression>()->callee;
      if (call_expr != nullptr && r.Chance(kCallOptionalCallChance()) &&
          ToggleOptionalChain(b, call_expr, ctx)) {
        return true;
      }
      if (!args->empty() && r.Chance(kCallSpreadToggleChance())) {
        std::vector<size_t> preferred;
        for (size_t i = 0; i < args->size(); i++) {
          if (IsSpreadPreferredArg(b, (*args)[i])) {
            preferred.push_back(i);
          }
        }
        const size_t at =
            !preferred.empty() ? r.Pick(preferred) : r.Index(args->size());
        if (auto* spread = (*args)[at]->As<SpreadElement>()) {
          (*args)[at] = spread->argument;
          ctx.detail = "unwrapped spread argument";
        } else {
          SpreadElement* wrapped = mk::Spread(t, (*args)[at]);
          if (node->start >= 0) {
            wrapped->start = wrapped->end = node->start;
          }
          (*args)[at] = wrapped;
          ctx.detail = "spread argument";
        }
        b.Invalidate();
        return true;
      }
      GenContext gen = ContextForNode(b, node, ctx);
      CodeGen cg(gen);
      switch (r.Index(6)) {
        case 0:
        case 1: {
          size_t at = r.Index(args->size() + 1);
          args->insert(args->begin() + static_cast<std::ptrdiff_t>(at),
                       cg.Value(JsType::kUnknown, 1));
          ctx.detail = "added argument";
          break;
        }
        case 2: {
          if (args->empty()) {
            continue;
          }
          size_t at = r.Index(args->size());
          Expression* old = (*args)[at];
          if (old->Is<SpreadElement>()) {
            continue;
          }
          (*args)[at] =
              r.Coin()
                  ? static_cast<Expression*>(mk::Spread(t, mk::Array(t, {old})))
                  : cg.Value(JsType::kUnknown, 1);
          ctx.detail = "replaced argument";
          break;
        }
        case 3: {
          Identifier* f = cg.Variable(JsType::kFunction);
          if (f == nullptr) {
            f = cg.Variable(JsType::kClass);
          }
          if (f == nullptr) {
            continue;
          }
          if (callee->Is<Identifier>() &&
              callee->Cast<Identifier>()->name == f->name) {
            continue;
          }
          ctx.detail = "callee -> " + std::string(f->name);
          callee = f;
          if (call_expr != nullptr) {
            if (auto* chain = ChainTop(b, call_expr)->As<ChainExpression>()) {
              UnwrapEmptyChain(b, chain);
            }
          }
          break;
        }
        case 4: {
          // call <-> new
          if (call_expr != nullptr) {
            if (call_expr->optional || parent->Is<ChainExpression>() ||
                SpineHasOptional(call_expr) || callee->Is<ChainExpression>() ||
                callee->Is<ArrowFunctionExpression>()) {
              continue;
            }
            NewExpression* n = mk::New(t, callee, *args);
            if (ReplaceAnchored(b, node, n) == nullptr) {
              continue;
            }
            ctx.detail = "call -> new";
          } else {
            CallExpression* c = mk::Call(t, callee, *args);
            if (ReplaceAnchored(b, node, c) == nullptr) {
              continue;
            }
            ctx.detail = "new -> call";
          }
          return true;
        }
        default: {
          if (call_expr == nullptr || call_expr->optional ||
              parent->Is<ChainExpression>() || SpineHasOptional(call_expr)) {
            continue;
          }
          // f(args) -> f.call(thisArg, args) / f.bind(thisArg, args)
          std::vector<Expression*> new_args = {cg.ObjectLike()};
          new_args.insert(new_args.end(), args->begin(), args->end());
          CallExpression* c = mk::CallMethod(
              t, callee, r.Coin() ? "call" : "bind", std::move(new_args));
          if (ReplaceAnchored(b, node, c) == nullptr) {
            continue;
          }
          ctx.detail = "call -> .call/.bind";
          return true;
        }
      }
      b.Invalidate();
      return true;
    }
    return false;
  }
};

// --- Splice mutator
// ---------------------------------------------------------------------

REDLINE_TUNABLE(kSpliceMinDonorNodes, "mutators.Splice.min_donor_nodes", 2, 1,
                1024,
                "fewest AST nodes a donor statement must have to be spliced");
REDLINE_TUNABLE(kSpliceMaxDonorNodes, "mutators.Splice.max_donor_nodes", 400, 1,
                4096,
                "most AST nodes a donor statement may have to be spliced");
REDLINE_TUNABLE(kSpliceMaxAttempts, "mutators.Splice.max_attempts", 6, 1, 16,
                "donor statements the Splice mutator tries before giving up");
REDLINE_TUNABLE(
    kSpliceSlotAttempts, "mutators.Splice.slot_attempts", 8, 1, 16,
    "insertion slots drawn to find one compatible with the donor statement");
REDLINE_TUNABLE(kSpliceAnyTypeRewireChance,
                "mutators.Splice.any_type_rewire_chance", 0.5, 0, 1,
                "probability of rewiring an external input to a variable of "
                "any type when none of its type is visible");
REDLINE_TUNABLE(kSpliceSliceChance, "mutators.Splice.slice_chance", 0.6, 0, 1,
                "probability of splicing earlier statements from the donor "
                "block that feed the spliced statement");
REDLINE_TUNABLE(kSpliceMaxSliceStatements,
                "mutators.Splice.max_slice_statements", 3, 0, 16,
                "most preceding statements from the donor block included in a "
                "slice");
REDLINE_TUNABLE(kSpliceMutatingStatementChance,
                "mutators.Splice.mutating_statement_chance", 0.5, 0, 1,
                "probability of including an earlier statement that uses or "
                "mutates a needed variable");

// Clones a statement from a donor tree into the destination, renaming the
// donor's own bindings to avoid clashes and rewiring references to
// variables declared outside the donor statement to variables visible at
// the destination (or fresh declarations).
class SpliceMutator final : public Mutator {
 public:
  // Fuzzilli: SpliceMutator, weight 2, up to 7 simultaneous.
  SpliceMutator()
      : Mutator(
            "Splice",
            "copies a statement from another program and rewires its inputs", 2,
            true, 7) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    if (ctx.partner == nullptr) {
      return false;
    }
    Random& r = b.rng();
    if (ctx.partner_cache == nullptr ||
        ctx.partner_cache->partner.get() != ctx.partner.get()) {
      auto cache = std::make_unique<MutationContext::PartnerCache>();
      cache->partner = ctx.partner;
      cache->donor_tree = ctx.partner->CopyTree();
      cache->donor_rng = std::make_unique<Random>(r.Fork());
      cache->donor = std::make_unique<Builder>(cache->donor_tree.get(),
                                               cache->donor_rng.get(), ctx.env);
      cache->candidates = cache->donor->Collect([&](Node* n) {
        if (!n->IsStatement()) {
          return false;
        }
        auto* s = n->Cast<Statement>();
        if (IsDirective(s) || s->Is<EmptyStatement>() ||
            s->Is<BreakStatement>() || s->Is<ContinueStatement>() ||
            s->Is<ReturnStatement>() || s->Is<DebuggerStatement>()) {
          return false;
        }
        // Loop heads (`for (const e of ...)`) are declarations without
        // initializers: not statements on their own.
        if (s->Is<VariableDeclaration>()) {
          Node* parent = cache->donor->ParentOf(n);
          if (parent != nullptr &&
              (parent->Is<ForStatement>() || parent->Is<ForInStatement>() ||
               parent->Is<ForOfStatement>())) {
            return false;
          }
        }
        // Code using a private name of its class cannot leave it.
        if (UsesPrivateNameOfEnclosingClass(n)) {
          return false;
        }
        size_t size = CountNodes(n);
        size_t min_nodes = static_cast<size_t>(kSpliceMinDonorNodes.AsInt());
        size_t max_nodes = static_cast<size_t>(kSpliceMaxDonorNodes.AsInt());
        return size >= min_nodes && size <= max_nodes;
      });
      ctx.partner_cache = std::move(cache);
    }
    if (ctx.partner_cache->candidates.empty()) {
      return false;
    }
    Builder& donor = *ctx.partner_cache->donor;
    const std::vector<Node*>& candidates = ctx.partner_cache->candidates;
    for (int attempt = 0; attempt < kSpliceMaxAttempts.AsInt(); attempt++) {
      Node* donor_node = candidates[r.Index(candidates.size())];
      Requirements req = AnalyzeRequirements(donor_node);
      // Find a compatible insertion slot.
      Builder::Slot slot;
      bool found = false;
      for (int i = 0; i < kSpliceSlotAttempts.AsInt(); i++) {
        slot = b.RandomInsertionSlot();
        if (!slot.valid()) {
          return false;
        }
        if (Compatible(req, b.ContextAtSlot(slot))) {
          found = true;
          break;
        }
      }
      if (!found) {
        continue;
      }
      if (Splice(b, donor, donor_node->Cast<Statement>(), slot, ctx)) {
        return true;
      }
    }
    return false;
  }

 private:
  bool Splice(Builder& b, Builder& donor, Statement* donor_stmt,
              const Builder::Slot& slot, MutationContext& ctx) {
    Random& r = b.rng();
    Tree* t = b.tree();
    ScopeAnalysis& donor_scopes = donor.scopes();
    std::vector<Statement*> slice = {donor_stmt};
    const int max_extra = kSpliceMaxSliceStatements.AsInt();
    if (max_extra > 0 && r.Chance(kSpliceSliceChance())) {
      Builder::Slot donor_slot = donor.SlotOf(donor_stmt);
      if (donor_slot.valid() && donor_slot.index > 0) {
        Builder::Context dest_ctx = b.ContextAtSlot(slot);
        const size_t max_nodes =
            static_cast<size_t>(kSpliceMaxDonorNodes.AsInt());
        for (size_t idx = donor_slot.index;
             idx > 0 && static_cast<int>(slice.size()) - 1 < max_extra;) {
          absl::flat_hash_set<const Node*> slice_nodes;
          for (Statement* s : slice) {
            Walk(s, [&](Node* n) {
              slice_nodes.insert(n);
              return true;
            });
          }
          absl::flat_hash_set<Binding*> needed;
          for (Statement* s : slice) {
            Walk(s, [&](Node* n) {
              if (auto* id = n->As<Identifier>()) {
                Binding* binding = donor_scopes.BindingOf(id);
                if (binding != nullptr && binding->name != "arguments") {
                  bool declared_in_slice = false;
                  for (Identifier* d : binding->declarations) {
                    if (slice_nodes.count(d)) {
                      declared_in_slice = true;
                      break;
                    }
                  }
                  if (!declared_in_slice) {
                    needed.insert(binding);
                  }
                }
              }
              return true;
            });
          }
          if (needed.empty()) {
            break;
          }
          Statement* prev = (*donor_slot.list)[--idx];
          if (IsDirective(prev) || prev->Is<EmptyStatement>() ||
              prev->Is<BreakStatement>() || prev->Is<ContinueStatement>() ||
              prev->Is<ReturnStatement>() || prev->Is<DebuggerStatement>()) {
            continue;
          }
          if (auto* vd = prev->As<VariableDeclaration>()) {
            if (vd->declaration_kind == VariableKind::kUsing ||
                vd->declaration_kind == VariableKind::kAwaitUsing) {
              continue;
            }
          }
          if (UsesPrivateNameOfEnclosingClass(prev) ||
              CountNodes(prev) > max_nodes ||
              !Compatible(AnalyzeRequirements(prev), dest_ctx)) {
            continue;
          }
          bool declares_needed = false;
          bool uses_needed = false;
          Walk(prev, [&](Node* n) {
            if (auto* id = n->As<Identifier>()) {
              Binding* binding = donor_scopes.BindingOf(id);
              if (binding != nullptr && needed.count(binding)) {
                if (std::find(binding->declarations.begin(),
                              binding->declarations.end(),
                              id) != binding->declarations.end()) {
                  declares_needed = true;
                } else {
                  uses_needed = true;
                }
              }
            }
            return true;
          });
          if (declares_needed ||
              (uses_needed && r.Chance(kSpliceMutatingStatementChance()))) {
            slice.insert(slice.begin(), prev);
          }
        }
      }
    }
    // Nodes inside the donor slice.
    absl::flat_hash_set<const Node*> inside;
    for (Statement* s : slice) {
      Walk(s, [&](Node* n) {
        inside.insert(n);
        return true;
      });
    }
    // Clone and map donor nodes to clone nodes.
    std::vector<Statement*> clones;
    clones.reserve(slice.size());
    absl::flat_hash_map<const Node*, Node*> to_clone;
    size_t total_nodes = 0;
    for (Statement* s : slice) {
      Statement* clone = t->Clone(s);
      clones.push_back(clone);
      ParallelWalk(s, clone, [&](Node* a, Node* c) { to_clone[a] = c; });
      total_nodes += CountNodes(s);
    }
    // Classify bindings touched by the donor. They are collected in the
    // order of their first occurrence in the (source-ordered) walk so that
    // the renaming below is deterministic for a given seed.
    std::vector<Binding*> internal, external;
    absl::flat_hash_set<Binding*> seen;
    for (Statement* s : slice) {
      Walk(s, [&](Node* n) {
        if (auto* id = n->As<Identifier>()) {
          Binding* binding = donor_scopes.BindingOf(id);
          if (binding == nullptr || !seen.insert(binding).second) {
            return true;
          }
          bool declared_inside = false;
          for (Identifier* d : binding->declarations) {
            if (inside.count(d)) {
              declared_inside = true;
            }
          }
          (declared_inside ? internal : external).push_back(binding);
        }
        return true;
      });
    }
    // Names in the destination and in the donor must not collide with fresh
    // names.
    b.ReserveNames(donor_scopes.all_names());
    const absl::flat_hash_set<std::string_view>& dest_names =
        b.scopes().all_names();
    absl::flat_hash_set<std::string_view> used_names(dest_names.begin(),
                                                     dest_names.end());
    std::vector<Variable> visible = b.VisibleAtSlot(slot);
    auto rename = [&](Binding* binding, std::string_view new_name) {
      std::string_view interned = t->Intern(new_name);
      for (Identifier* d : binding->declarations) {
        auto it = to_clone.find(d);
        if (it != to_clone.end()) {
          it->second->Cast<Identifier>()->name = interned;
        }
      }
      for (Identifier* ref : binding->references) {
        auto it = to_clone.find(ref);
        if (it != to_clone.end()) {
          it->second->Cast<Identifier>()->name = interned;
        }
      }
    };
    int renamed = 0, rewired = 0, declared = 0;
    for (Binding* binding : internal) {
      if (binding->name == "arguments") {
        continue;
      }
      if (used_names.count(binding->name)) {
        std::string_view fresh = b.FreshName(binding->name);
        rename(binding, fresh);
        used_names.insert(fresh);
        renamed++;
      } else {
        used_names.insert(binding->name);
      }
    }
    std::vector<Statement*> prologue;
    for (Binding* binding : external) {
      if (binding->name == "arguments") {
        continue;
      }
      JsType type = donor.GuessType(binding);
      std::vector<const Variable*> options;
      for (const Variable& v : visible) {
        if (v.name == "arguments") {
          continue;
        }
        if (binding->assigned && v.is_constant) {
          continue;
        }
        if (type == JsType::kUnknown || v.type == type) {
          options.push_back(&v);
        }
      }
      if (options.empty() && r.Chance(kSpliceAnyTypeRewireChance())) {
        for (const Variable& v : visible) {
          if (v.name == "arguments" || (binding->assigned && v.is_constant)) {
            continue;
          }
          options.push_back(&v);
        }
      }
      if (!options.empty()) {
        const Variable& choice = *options[r.Index(options.size())];
        rename(binding, choice.name);
        rewired++;
      } else {
        // Declare a stand-in before the spliced statement.
        std::string_view name = used_names.count(binding->name)
                                    ? b.FreshName(binding->name)
                                    : t->Intern(binding->name);
        used_names.insert(name);
        if (name != binding->name) {
          rename(binding, name);
        }
        GenContext gen = ContextForSlot(b, slot, ctx);
        CodeGen cg(gen);
        prologue.push_back(mk::Let(t, name, cg.Literal(type)));
        declared++;
      }
    }
    for (Statement* c : clones) {
      prologue.push_back(c);
    }
    b.InsertAll(slot, prologue);
    ctx.detail = StringPrintf(
        "spliced %s (%zu stmts, %zu nodes; %d renamed, %d rewired, %d "
        "declared)",
        donor_stmt->kind_name(), slice.size(), total_nodes, renamed, rewired,
        declared);
    return true;
  }
};

// --- Combine mutator
// -----------------------------------------------------------------------

REDLINE_TUNABLE(kCombineIifeChance, "mutators.Combine.iife_chance", 0.5, 0, 1,
                "probability of merging the partner program as an IIFE rather "
                "than flat at the top level");
REDLINE_TUNABLE(kCombineSlotAttempts, "mutators.Combine.slot_attempts", 8, 1,
                16,
                "insertion slots drawn to find one whose strictness suits the "
                "partner program");
REDLINE_TUNABLE(kCombineIifeAtEndChance, "mutators.Combine.iife_at_end_chance",
                0.6, 0, 1,
                "probability that the IIFE goes at the end of the program "
                "rather than at a random slot");
REDLINE_TUNABLE(kCombineFlatAtEndChance, "mutators.Combine.flat_at_end_chance",
                0.5, 0, 1,
                "probability that the flat merge is appended at the end rather "
                "than at a random top-level index");

class CombineMutator final : public Mutator {
 public:
  // Fuzzilli: CombineMutator, weight 1, one program per application.
  CombineMutator()
      : Mutator("Combine",
                "merges another whole program in (as an IIFE or flat with "
                "renaming)",
                1, true) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    if (ctx.partner == nullptr) {
      return false;
    }
    Random& r = b.rng();
    std::unique_ptr<Tree> donor_tree = ctx.partner->CopyTree();
    Random donor_rng = r.Fork();
    const size_t max_combine_nodes = MaterialMaxCombineNodes();
    if (ctx.partner->node_count() > max_combine_nodes) {
      UnwrapTopLevelIIFEs(donor_tree.get());
      if (CountNodes(donor_tree->root()) > max_combine_nodes &&
          !ExtractCompactWindow(donor_tree.get(), max_combine_nodes,
                                donor_rng)) {
        return false;
      }
    }
    Builder donor(donor_tree.get(), &donor_rng, ctx.env);
    fast::Program* donor_root = donor.root();
    if (donor_root->body.empty()) {
      return false;
    }
    Requirements req = AnalyzeRequirements(donor_root);
    Tree* t = b.tree();
    if (b.focus_subtree() != nullptr || r.Chance(kCombineIifeChance())) {
      // IIFE: `(function() { <donor> })();` at a random insertion slot.
      Builder::Slot slot;
      bool found = false;
      for (int i = 0; i < kCombineSlotAttempts.AsInt(); i++) {
        slot = (b.focus_subtree() == nullptr &&
                r.Chance(kCombineIifeAtEndChance()))
                   ? Builder::Slot{&b.root()->body, b.root()->body.size(),
                                   b.root()}
                   : b.RandomInsertionSlot();
        if (!slot.valid()) {
          return false;
        }
        Builder::Context c = b.ContextAtSlot(slot);
        if (!req.needs_sloppy || !c.strict) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      std::vector<Statement*> body;
      body.reserve(donor_root->body.size());
      for (Statement* s : donor_root->body) {
        body.push_back(t->Clone(s));
      }
      Expression* fn = mk::Function(t, "", {}, std::move(body));
      b.Insert(slot, mk::ExprStmt(t, mk::Call(t, fn, {})));
      ctx.detail = StringPrintf("combined as IIFE (%zu statements)",
                                donor_root->body.size());
      return true;
    }
    // Flat merge into the top level, renaming clashing top-level bindings.
    if (req.needs_sloppy && b.root()->strict) {
      return false;
    }
    ScopeAnalysis& donor_scopes = donor.scopes();
    b.ReserveNames(donor_scopes.all_names());
    const absl::flat_hash_set<std::string_view>& dest_names =
        b.scopes().all_names();
    int renamed = 0;
    std::vector<Binding*> clashing;
    for (Binding* binding : donor_scopes.root()->bindings) {
      if (dest_names.count(binding->name)) {
        clashing.push_back(binding);
      }
    }
    for (Binding* binding : clashing) {
      RenameBinding(donor_tree.get(), binding, b.FreshName(binding->name));
      renamed++;
    }
    std::vector<Statement*> statements;
    for (Statement* s : donor_root->body) {
      if (IsDirective(s)) {
        continue;
      }
      statements.push_back(t->Clone(s));
    }
    if (statements.empty()) {
      return false;
    }
    std::vector<Statement*>& dest = b.root()->body;
    size_t first = 0;
    while (first < dest.size() && IsDirective(dest[first])) {
      first++;
    }
    size_t index =
        r.Chance(kCombineFlatAtEndChance())
            ? dest.size()
            : static_cast<size_t>(r.Range(static_cast<int64_t>(first),
                                          static_cast<int64_t>(dest.size())));
    Builder::Slot slot{&dest, index, b.root()};
    b.InsertAll(slot, statements);
    ctx.detail = StringPrintf("combined flat (%zu statements, %d renamed)",
                              statements.size(), renamed);
    return true;
  }
};

// --- CallSiteStress mutator ------------------------------------------------

REDLINE_TUNABLE(
    kCallSiteStressMaxAttempts, "mutators.CallSiteStress.max_attempts", 8, 1,
    32, "candidate call sites that CallSiteStress tries before giving up");
REDLINE_TUNABLE(kCallSiteStressMaxLoopDepth,
                "mutators.CallSiteStress.max_loop_depth", 2, 0, 8,
                "maximum enclosing loop depth for a CallSiteStress site");
REDLINE_TUNABLE(kCallSiteStressMaxLoopBound,
                "mutators.CallSiteStress.max_loop_bound", 200, 1, 10000,
                "largest constant loop bound that CallSiteStress enters");
REDLINE_TUNABLE(kCallSiteStressTierUpWeight,
                "mutators.CallSiteStress.tier_up_weight", 3.0, 0.0, 100.0,
                "weight of the JIT tier-up call stress pattern");
REDLINE_TUNABLE(kCallSiteStressAllocTimeoutWeight,
                "mutators.CallSiteStress.alloc_timeout_weight", 2.0, 0.0, 100.0,
                "weight of the post-optimize allocation timeout GC pattern");
REDLINE_TUNABLE(kCallSiteStressDeoptRetierWeight,
                "mutators.CallSiteStress.deopt_retier_weight", 2.0, 0.0, 100.0,
                "weight of the deoptimize or baseline re-tier pattern");
REDLINE_TUNABLE(kCallSiteStressStackLimitWeight,
                "mutators.CallSiteStress.stack_limit_weight", 1.5, 0.0, 100.0,
                "weight of the near-stack-limit call wrapper pattern");
REDLINE_TUNABLE(kCallSiteStressReentrantWeight,
                "mutators.CallSiteStress.reentrant_weight", 1.5, 0.0, 100.0,
                "weight of the re-entrant accessor or shape change pattern");
REDLINE_TUNABLE(kCallSiteStressWarmupCalls,
                "mutators.CallSiteStress.warmup_calls", 2, 1, 8,
                "warm-up calls before optimization in CallSiteStress");
REDLINE_TUNABLE(kCallSiteStressMaglevChance,
                "mutators.CallSiteStress.maglev_chance", 0.35, 0.0, 1.0,
                "probability that tier-up chooses %OptimizeMaglevOnNextCall");
REDLINE_TUNABLE(
    kCallSiteStressAllocTimeoutMax, "mutators.CallSiteStress.alloc_timeout_max",
    3000, 1, 10000,
    "maximum allocation count before allocation timeout triggers GC");
REDLINE_TUNABLE(kCallSiteStressCompileBaselineChance,
                "mutators.CallSiteStress.compile_baseline_chance", 0.4, 0.0,
                1.0, "probability that deopt re-tier chooses %CompileBaseline");
REDLINE_TUNABLE(kCallSiteStressStackDepth,
                "mutators.CallSiteStress.stack_depth", 16384, 256, 65536,
                "recursion depth cap of the near-stack-limit helper");
REDLINE_TUNABLE(kCallSiteStressStackMaxCalls,
                "mutators.CallSiteStress.stack_max_calls", 4, 1, 32,
                "maximum stack-limit probe executions per script run");
REDLINE_TUNABLE(kCallSiteStressStackMaxRetries,
                "mutators.CallSiteStress.stack_max_retries", 512, 0, 4096,
                "maximum RangeError retry frames in the stack-limit helper");
REDLINE_TUNABLE(kCallSiteStressReentrantDepth,
                "mutators.CallSiteStress.reentrant_depth", 2, 1, 4,
                "entry count budget for a generated re-entrant accessor");
REDLINE_TUNABLE(
    kCallSiteStressAccessorChance, "mutators.CallSiteStress.accessor_chance",
    0.6, 0.0, 1.0,
    "probability of installing an accessor instead of changing shape");
REDLINE_TUNABLE(kCallSiteStressGcChance, "mutators.CallSiteStress.gc_chance",
                0.25, 0.0, 1.0,
                "probability of inserting gc() during re-entrant perturbation");

constexpr std::string_view kNearStackHelperName = "__redline_near_stack";

bool IsHarnessGlobal(std::string_view name) {
  return name == "eval" || name == "quit" || name == "load" ||
         name == "readline" || name == "print" || name == "printErr" ||
         name == "gc" || name == "d8" || name == "Realm" || name == "os" ||
         name == "setTimeout" || name == "version" || name == "performance" ||
         name == "Worker" || name == "read" || name == "readbuffer" ||
         name == "writeFile" || name == "isNeverOptimizeLite" ||
         name == "isInterpreted" || name == "isOptimized" ||
         name == "isMaglevved" || name == "isTurboFanned" || name == "inIon" ||
         name == "inBaseline" || name.starts_with("assert");
}

// Walks a simple member chain and sets `root` to the base `Identifier`.
bool AnalyzeReceiverChain(const Expression* expr, const Identifier** root) {
  *root = nullptr;
  while (expr != nullptr) {
    if (auto* id = expr->As<Identifier>()) {
      if (id->name == "eval") {
        return false;
      }
      *root = id;
      return true;
    }
    if (expr->Is<ThisExpression>()) {
      return true;
    }
    if (auto* mem = expr->As<MemberExpression>()) {
      if (mem->computed || mem->optional || !mem->property->Is<Identifier>()) {
        return false;
      }
      expr = mem->object;
      continue;
    }
    return false;
  }
  return false;
}

// Gives the property next to the base of a chain that
// `AnalyzeReceiverChain` accepts.
std::string_view FirstChainProperty(const MemberExpression* mem) {
  while (auto* inner = mem->object->As<MemberExpression>()) {
    mem = inner;
  }
  return mem->property->Cast<Identifier>()->name;
}

bool IsSimpleCallee(Builder& b, const Expression* callee) {
  if (auto* id = callee->As<Identifier>()) {
    if (id->name == "eval" || id->name.starts_with(kNearStackHelperName)) {
      return false;
    }
    if (b.scopes().BindingOf(id) == nullptr && IsHarnessGlobal(id->name)) {
      return false;
    }
    return true;
  }
  if (auto* mem = callee->As<MemberExpression>()) {
    const Identifier* root = nullptr;
    if (mem->computed || mem->optional || !mem->property->Is<Identifier>() ||
        !AnalyzeReceiverChain(mem->object, &root)) {
      return false;
    }
    const bool unbound_root =
        root != nullptr && b.scopes().BindingOf(root) == nullptr;
    if (unbound_root && IsHarnessGlobal(root->name)) {
      return false;
    }
    // `globalThis.quit()` and `this.print()` can reach shell globals too.
    if ((root == nullptr || (unbound_root && root->name == "globalThis")) &&
        IsHarnessGlobal(FirstChainProperty(mem))) {
      return false;
    }
    return true;
  }
  return false;
}

bool IsTrueCondition(const Expression* expr) {
  if (expr == nullptr) {
    return false;
  }
  if (auto* lit = expr->As<BooleanLiteral>()) {
    return lit->value;
  }
  if (auto* num = expr->As<NumericLiteral>()) {
    return num->value != 0.0 && !std::isnan(num->value);
  }
  if (auto* str = expr->As<StringLiteral>()) {
    return !str->value.empty();
  }
  return false;
}

bool NumericBound(const Expression* expr, double* out) {
  if (expr == nullptr) {
    return false;
  }
  if (auto* num = expr->As<NumericLiteral>()) {
    if (std::isfinite(num->value)) {
      *out = num->value;
      return true;
    }
    return false;
  }
  if (auto* un = expr->As<UnaryExpression>()) {
    double inner = 0.0;
    if (!NumericBound(un->argument, &inner)) {
      return false;
    }
    if (un->op == UnaryOp::kMinus) {
      *out = -inner;
      return true;
    }
    if (un->op == UnaryOp::kPlus) {
      *out = inner;
      return true;
    }
  }
  return false;
}

bool IsComparisonOp(BinaryOp op) {
  return op == BinaryOp::kLessThan || op == BinaryOp::kLessThanEq ||
         op == BinaryOp::kGreaterThan || op == BinaryOp::kGreaterThanEq ||
         op == BinaryOp::kNotEq || op == BinaryOp::kStrictNotEq;
}

bool ExtractForInitValue(const Node* init, std::string_view var, double* out) {
  if (init == nullptr) {
    return false;
  }
  if (auto* decl = init->As<VariableDeclaration>()) {
    for (const VariableDeclarator* d : decl->declarations) {
      if (auto* id = d->target->As<Identifier>()) {
        if (id->name == var && NumericBound(d->init, out)) {
          return true;
        }
      }
    }
  } else if (auto* assign = init->As<AssignmentExpression>()) {
    if (auto* id = assign->left->As<Identifier>()) {
      if (id->name == var && NumericBound(assign->right, out)) {
        return true;
      }
    }
  }
  return false;
}

bool IsHugeOrInfiniteLoop(const Node* node, double max_bound) {
  if (auto* fs = node->As<ForStatement>()) {
    if (fs->test == nullptr || IsTrueCondition(fs->test)) {
      return true;
    }
    auto* bin = fs->test->As<BinaryExpression>();
    if (bin == nullptr || !IsComparisonOp(bin->op)) {
      return false;
    }
    double bound = 0.0;
    const Identifier* var = nullptr;
    if (auto* l = bin->left->As<Identifier>()) {
      if (NumericBound(bin->right, &bound)) {
        var = l;
      }
    } else if (auto* r = bin->right->As<Identifier>()) {
      if (NumericBound(bin->left, &bound)) {
        var = r;
      }
    }
    if (var == nullptr) {
      return false;
    }
    double init_val = 0.0;
    if (ExtractForInitValue(fs->init, var->name, &init_val)) {
      return std::abs(bound - init_val) > max_bound;
    }
    return std::abs(bound) > max_bound;
  }
  if (auto* ws = node->As<WhileStatement>()) {
    return IsTrueCondition(ws->test);
  }
  if (auto* dws = node->As<DoWhileStatement>()) {
    return IsTrueCondition(dws->test);
  }
  return false;
}

bool IsLoopNode(const Node* node) {
  return node->Is<ForStatement>() || node->Is<ForInStatement>() ||
         node->Is<ForOfStatement>() || node->Is<WhileStatement>() ||
         node->Is<DoWhileStatement>();
}

std::string_view EnclosingFunctionName(Builder& b, const Node* fn) {
  if (auto* fd = fn->As<FunctionDeclaration>()) {
    return fd->name != nullptr ? fd->name->name : std::string_view{};
  }
  if (auto* fe = fn->As<FunctionExpression>()) {
    if (fe->name != nullptr) {
      return fe->name->name;
    }
  }
  const Node* parent = b.ParentOf(const_cast<Node*>(fn));
  if (parent == nullptr) {
    return {};
  }
  if (auto* decl = parent->As<VariableDeclarator>()) {
    if (auto* id = decl->target->As<Identifier>()) {
      return id->name;
    }
  }
  if (auto* assign = parent->As<AssignmentExpression>()) {
    if (assign->right == fn) {
      if (auto* id = assign->left->As<Identifier>()) {
        return id->name;
      }
    }
  }
  return {};
}

bool IsInsideNearStackHelper(Builder& b, Node* node) {
  Node* cur = b.ParentOf(node);
  while (cur != nullptr) {
    if (cur->IsFunction() &&
        EnclosingFunctionName(b, cur).starts_with(kNearStackHelperName)) {
      return true;
    }
    cur = b.ParentOf(cur);
  }
  return false;
}

std::string_view PropertyKeyName(const Expression* key, bool computed) {
  if (computed || key == nullptr) {
    return {};
  }
  if (auto* id = key->As<Identifier>()) {
    return id->name;
  }
  if (auto* str = key->As<StringLiteral>()) {
    return str->value;
  }
  return {};
}

std::string_view EnclosingMethodName(Builder& b, const Node* fn) {
  const Node* parent = b.ParentOf(const_cast<Node*>(fn));
  if (parent == nullptr) {
    return {};
  }
  if (auto* md = parent->As<MethodDefinition>()) {
    if (md->value == fn) {
      return PropertyKeyName(md->key, md->computed);
    }
  } else if (auto* prop = parent->As<Property>()) {
    if (prop->value == fn) {
      return PropertyKeyName(prop->key, prop->computed);
    }
  } else if (auto* assign = parent->As<AssignmentExpression>()) {
    if (assign->right == fn) {
      if (auto* mem = assign->left->As<MemberExpression>()) {
        if (!mem->computed) {
          return PropertyKeyName(mem->property, false);
        }
      }
    }
  }
  return {};
}

bool MatchesEnclosingNamedReceiver(Builder& b, const Node* fn,
                                   std::string_view receiver_name) {
  Node* owner = b.ParentOf(const_cast<Node*>(fn));
  if (owner == nullptr) {
    return false;
  }
  if (auto* md = owner->As<MethodDefinition>()) {
    if (!md->is_static) {
      return false;
    }
    Node* body = b.ParentOf(md);
    Node* cls = body != nullptr ? b.ParentOf(body) : nullptr;
    const ClassData* data = cls != nullptr ? AsClassData(cls) : nullptr;
    return data != nullptr && data->name != nullptr &&
           data->name->name == receiver_name;
  }
  if (auto* prop = owner->As<Property>()) {
    Node* obj = b.ParentOf(prop);
    Node* obj_parent = obj != nullptr ? b.ParentOf(obj) : nullptr;
    if (auto* decl = obj_parent != nullptr
                         ? obj_parent->As<VariableDeclarator>()
                         : nullptr) {
      auto* id = decl->target->As<Identifier>();
      return id != nullptr && id->name == receiver_name;
    }
    if (auto* assign = obj_parent != nullptr
                           ? obj_parent->As<AssignmentExpression>()
                           : nullptr) {
      auto* id = assign->left->As<Identifier>();
      return assign->right == obj && id != nullptr && id->name == receiver_name;
    }
    return false;
  }
  if (auto* assign = owner->As<AssignmentExpression>()) {
    if (assign->right == fn) {
      if (auto* mem = assign->left->As<MemberExpression>()) {
        auto* id = mem->object->As<Identifier>();
        return id != nullptr && id->name == receiver_name;
      }
    }
  }
  return false;
}

bool IsSelfRecursiveCall(Builder& b, CallExpression* call) {
  if (auto* id = call->callee->As<Identifier>()) {
    Node* cur = b.ParentOf(call);
    while (cur != nullptr) {
      if (cur->IsFunction() && EnclosingFunctionName(b, cur) == id->name) {
        return true;
      }
      cur = b.ParentOf(cur);
    }
    return false;
  }
  auto* mem = call->callee->As<MemberExpression>();
  if (mem == nullptr || mem->computed || mem->optional) {
    return false;
  }
  auto* prop = mem->property->As<Identifier>();
  if (prop == nullptr) {
    return false;
  }
  if (mem->object->Is<ThisExpression>()) {
    Node* cur = b.ParentOf(call);
    while (cur != nullptr) {
      if (cur->IsClass() || cur->Is<PropertyDefinition>() ||
          cur->Is<StaticBlock>()) {
        return false;
      }
      if (cur->IsFunction()) {
        if (EnclosingMethodName(b, cur) == prop->name) {
          return true;
        }
        // Non-arrow functions bind `this`.
        if (!AsFunctionData(cur)->is_arrow) {
          return false;
        }
      }
      cur = b.ParentOf(cur);
    }
    return false;
  }
  if (auto* recv = mem->object->As<Identifier>()) {
    Node* cur = b.ParentOf(call);
    while (cur != nullptr) {
      if (cur->IsFunction() && EnclosingMethodName(b, cur) == prop->name &&
          MatchesEnclosingNamedReceiver(b, cur, recv->name)) {
        return true;
      }
      cur = b.ParentOf(cur);
    }
  }
  return false;
}

bool IsSingleStatementControlBody(Builder& b, Statement* stmt) {
  Node* parent = b.ParentOf(stmt);
  if (parent == nullptr) {
    return false;
  }
  if (auto* fs = parent->As<ForStatement>()) {
    return fs->body == stmt;
  }
  if (auto* fis = parent->As<ForInStatement>()) {
    return fis->body == stmt;
  }
  if (auto* fos = parent->As<ForOfStatement>()) {
    return fos->body == stmt;
  }
  return parent->Is<IfStatement>() || parent->Is<WhileStatement>() ||
         parent->Is<DoWhileStatement>() || parent->Is<LabeledStatement>() ||
         parent->Is<WithStatement>();
}

bool IsInsideTryOrConditional(Builder& b, Node* node) {
  Node* prev = node;
  Node* cur = b.ParentOf(node);
  while (cur != nullptr) {
    if (cur->IsFunction() || cur->IsClass()) {
      return false;
    }
    if (cur->Is<TryStatement>() || cur->Is<IfStatement>() ||
        cur->Is<SwitchStatement>() || cur->Is<ChainExpression>()) {
      return true;
    }
    if (auto* logical = cur->As<LogicalExpression>()) {
      if (logical->right == prev) {
        return true;
      }
    } else if (auto* cond = cur->As<ConditionalExpression>()) {
      if (cond->test != prev) {
        return true;
      }
    } else if (auto* assign = cur->As<AssignmentExpression>()) {
      if (assign->right == prev && (assign->op == AssignOp::kOrAssign ||
                                    assign->op == AssignOp::kAndAssign ||
                                    assign->op == AssignOp::kNullishAssign)) {
        return true;
      }
    } else if (auto* pattern = cur->As<AssignmentPattern>()) {
      // A destructuring default runs only for an undefined value.
      if (pattern->right == prev) {
        return true;
      }
    }
    prev = cur;
    cur = b.ParentOf(cur);
  }
  return false;
}

bool CanRunInSyncArrow(Node* node) {
  Requirements req = AnalyzeRequirements(node);
  return !req.needs_function && !req.needs_async && !req.needs_generator &&
         !req.needs_break && !req.needs_continue && !req.has_external_label;
}

bool IsObjectCandidateType(JsType type) {
  switch (type) {
    case JsType::kNumber:
    case JsType::kString:
    case JsType::kBoolean:
    case JsType::kBigInt:
    case JsType::kSymbol:
    case JsType::kNullish:
      return false;
    default:
      return true;
  }
}

bool IsReservedPropertyKey(std::string_view key) {
  return key.empty() || key == "__proto__" || key == "constructor" ||
         key == "prototype" || key == "length";
}

struct CallSite {
  CallExpression* call = nullptr;
  Statement* stmt = nullptr;
  bool known_function_binding = false;
  bool in_function = false;
  int loop_depth = 0;
  double score = 1.0;
};

Builder::Slot ObtainStmtSlot(Builder& b, Statement* stmt) {
  Builder::Slot slot = b.SlotOf(stmt);
  if (slot.valid()) {
    return slot;
  }
  return b.Blockify(stmt);
}

// Returns true when `node` lies inside `b.focus_subtree()`.
bool IsInsideFocus(Builder& b, const Node* node) {
  const Node* focus = b.focus_subtree();
  if (focus == nullptr) {
    return true;
  }
  const Node* cur = node;
  while (cur != nullptr) {
    if (cur == focus) {
      return true;
    }
    cur = b.ParentOf(const_cast<Node*>(cur));
  }
  return false;
}

// Returns true when `node` sits inside a callback passed to `Worker(...)`.
bool IsInsideWorkerCallback(Builder& b, Node* node) {
  Node* cur = node;
  while (cur != nullptr) {
    if (cur->IsFunction()) {
      Node* parent = b.ParentOf(cur);
      Expression* callee = nullptr;
      if (parent != nullptr) {
        if (auto* call = parent->As<CallExpression>()) {
          callee = call->callee;
        } else if (auto* nw = parent->As<NewExpression>()) {
          callee = nw->callee;
        }
      }
      if (auto* id = callee != nullptr ? callee->As<Identifier>() : nullptr;
          id != nullptr && id->name == "Worker") {
        return true;
      }
    }
    cur = b.ParentOf(cur);
  }
  return false;
}

Expression* CloneCallee(Tree* t, const CallExpression* call) {
  return static_cast<Expression*>(t->Clone(call->callee));
}

Statement* CloneCallStmt(Tree* t, const CallExpression* call) {
  return mk::ExprStmt(t, static_cast<CallExpression*>(t->Clone(call)));
}

bool ShouldGuardCallPrefix(Builder& b, const CallSite& site) {
  return !site.known_function_binding || IsInsideTryOrConditional(b, site.call);
}

std::vector<Statement*> BuildTierUpPrefix(Builder& b, const CallSite& site,
                                          bool guard, bool* used_maglev) {
  Tree* t = b.tree();
  Random& r = b.rng();
  *used_maglev = r.Chance(kCallSiteStressMaglevChance());
  std::string_view opt_name =
      *used_maglev ? "OptimizeMaglevOnNextCall" : "OptimizeFunctionOnNextCall";
  int warmup = kCallSiteStressWarmupCalls.AsInt();
  // Each warm-up clone runs the call again. In a function, calls that reach
  // each other through mutation can multiply the calls exponentially.
  if (site.loop_depth > 0 || site.in_function) {
    warmup = std::min(warmup, 1);
  }
  std::vector<Statement*> warmup_stmts;
  warmup_stmts.reserve(static_cast<size_t>(warmup));
  for (int i = 0; i < warmup; i++) {
    warmup_stmts.push_back(CloneCallStmt(t, site.call));
  }
  std::vector<Statement*> prefix;
  prefix.push_back(
      mk::ExprStmt(t, mk::Native(t, "PrepareFunctionForOptimization",
                                 {CloneCallee(t, site.call)})));
  if (guard) {
    // Isolates warm-up throws so the optimize native still runs.
    prefix.push_back(mk::Try(t, std::move(warmup_stmts), b.FreshName("e"), {}));
  } else {
    for (Statement* ws : warmup_stmts) {
      prefix.push_back(ws);
    }
  }
  prefix.push_back(
      mk::ExprStmt(t, mk::Native(t, opt_name, {CloneCallee(t, site.call)})));
  if (!guard) {
    return prefix;
  }
  return {mk::Try(t, std::move(prefix), b.FreshName("e"), {})};
}

std::vector<Statement*> BuildDeoptRetierPrefix(Builder& b, const CallSite& site,
                                               bool guard,
                                               bool* used_baseline) {
  Tree* t = b.tree();
  Random& r = b.rng();
  *used_baseline = r.Chance(kCallSiteStressCompileBaselineChance());
  std::vector<Statement*> prefix;
  if (*used_baseline) {
    prefix.push_back(mk::ExprStmt(
        t, mk::Native(t, "CompileBaseline", {CloneCallee(t, site.call)})));
  } else {
    prefix.push_back(CloneCallStmt(t, site.call));
    prefix.push_back(mk::ExprStmt(
        t, mk::Native(t, "DeoptimizeFunction", {CloneCallee(t, site.call)})));
  }
  if (!guard) {
    return prefix;
  }
  return {mk::Try(t, std::move(prefix), b.FreshName("e"), {})};
}

bool CanWrapInFinallyBlock(Builder& b, Statement* stmt) {
  if (b.EscapingBindings({stmt}, /*block_scoped_only=*/true).empty()) {
    return true;
  }
  auto* decl = stmt->As<VariableDeclaration>();
  return decl != nullptr && decl->declarations.size() == 1 &&
         decl->declarations[0]->target->Is<Identifier>();
}

TryStatement* BuildAllocTimeoutTry(Builder& b, Statement* inner_stmt,
                                   int64_t countdown, bool allow_inline) {
  Tree* t = b.tree();
  Statement* set_timeout =
      mk::ExprStmt(t, mk::Native(t, "SetAllocationTimeout",
                                 {mk::Unary(t, UnaryOp::kMinus, mk::Num(t, 1)),
                                  mk::Num(t, static_cast<double>(countdown)),
                                  mk::Bool(t, allow_inline)}));
  std::vector<Statement*> finalizer = {
      mk::ExprStmt(t, mk::Native(t, "SetAllocationTimeout",
                                 {mk::Unary(t, UnaryOp::kMinus, mk::Num(t, 1)),
                                  mk::Unary(t, UnaryOp::kMinus, mk::Num(t, 1)),
                                  mk::Bool(t, true)}))};
  TryStatement* try_stmt = t->New<TryStatement>();
  try_stmt->block = mk::Block(t, {set_timeout, inner_stmt});
  try_stmt->handler = nullptr;
  try_stmt->finalizer = mk::Block(t, std::move(finalizer));
  return try_stmt;
}

bool HasTopLevelNearStackHelper(Builder& b) {
  for (Statement* top : b.root()->body) {
    if (auto* fn = top->As<FunctionDeclaration>()) {
      if (fn->name != nullptr && fn->name->name == kNearStackHelperName &&
          fn->params.size() == 2) {
        return true;
      }
    }
  }
  return false;
}

std::string_view ChooseNearStackHelperName(Builder& b, bool* already_exists) {
  if (HasTopLevelNearStackHelper(b)) {
    *already_exists = true;
    return kNearStackHelperName;
  }
  *already_exists = false;
  return !b.scopes().all_names().count(kNearStackHelperName)
             ? kNearStackHelperName
             : b.FreshName(kNearStackHelperName);
}

void InsertNearStackHelper(Builder& b, std::string_view helper_name) {
  Tree* t = b.tree();
  double max_calls = std::max(1, kCallSiteStressStackMaxCalls.AsInt());
  double max_retries = std::max(0, kCallSiteStressStackMaxRetries.AsInt());

  // Builds a bounded recursive probe near the JS stack limit.
  Statement* c_decl =
      mk::Let(t, "c",
              mk::Binary(t, BinaryOp::kAdd,
                         mk::Logical(t, LogicalOp::kOr,
                                     mk::Member(t, mk::Id(t, helper_name), "c"),
                                     mk::Num(t, 0)),
                         mk::Num(t, 1)));
  Statement* c_store =
      mk::ExprStmt(t, mk::Assign(t, mk::Member(t, mk::Id(t, helper_name), "c"),
                                 mk::Id(t, "c")));
  Statement* call_cap =
      mk::If(t,
             mk::Binary(t, BinaryOp::kGreaterThan, mk::Id(t, "c"),
                        mk::Num(t, max_calls)),
             mk::Return(t, mk::Call(t, mk::Id(t, "f"), {})));
  Statement* retries_decl = mk::Let(t, "r", mk::Num(t, 0));

  Statement* depth_stop = mk::If(
      t,
      mk::Binary(t, BinaryOp::kLessThanEq,
                 mk::Update(t, UpdateOp::kDecrement, false, mk::Id(t, "d")),
                 mk::Num(t, 0)),
      mk::Return(t, mk::Call(t, mk::Id(t, "f"), {})));
  Statement* non_range_rethrow =
      mk::If(t,
             mk::Unary(t, UnaryOp::kNot,
                       mk::Binary(t, BinaryOp::kInstanceof, mk::Id(t, "e"),
                                  mk::Id(t, "RangeError"))),
             mk::Throw(t, mk::Id(t, "e")));
  Statement* step_decl = mk::Let(
      t, "k", mk::Update(t, UpdateOp::kIncrement, false, mk::Id(t, "r")));
  // Limits retry attempts to powers of two after unwinding stack frames.
  Statement* retry_guard = mk::If(
      t,
      mk::Logical(
          t, LogicalOp::kOr,
          mk::Binary(t, BinaryOp::kGreaterThan, mk::Id(t, "k"),
                     mk::Num(t, max_retries)),
          mk::Binary(t, BinaryOp::kStrictNotEq,
                     mk::Binary(t, BinaryOp::kBitAnd, mk::Id(t, "k"),
                                mk::Binary(t, BinaryOp::kSub, mk::Id(t, "k"),
                                           mk::Num(t, 1))),
                     mk::Num(t, 0))),
      mk::Throw(t, mk::Id(t, "e")));
  Statement* try_recurse =
      mk::Try(t, {mk::Return(t, mk::Call(t, mk::Id(t, "t"), {}))}, "e",
              {non_range_rethrow, step_decl, retry_guard,
               mk::Return(t, mk::Call(t, mk::Id(t, "f"), {}))});
  Statement* t_decl = mk::FunctionDecl(t, "t", {}, {depth_stop, try_recurse});
  Statement* ret_t = mk::Return(t, mk::Call(t, mk::Id(t, "t"), {}));

  FunctionDeclaration* helper = mk::FunctionDecl(
      t, helper_name, {"f", "d"},
      {c_decl, c_store, call_cap, retries_decl, t_decl, ret_t});
  size_t pos = 0;
  std::vector<Statement*>& body = b.root()->body;
  while (pos < body.size() && IsDirective(body[pos])) {
    pos++;
  }
  b.Insert(Builder::Slot{&body, pos, b.root()}, helper);
}

struct ObjectTarget {
  const Expression* expr = nullptr;
  std::string_view var_name;
};

std::vector<ObjectTarget> CollectObjectTargets(
    Builder& b, CallExpression* call, std::string_view* forbidden_key) {
  std::vector<ObjectTarget> targets;
  absl::flat_hash_set<std::string_view> seen_names;
  *forbidden_key = {};
  if (auto* mem = call->callee->As<MemberExpression>()) {
    *forbidden_key = mem->property->Cast<Identifier>()->name;
    if (!mem->object->Is<ThisExpression>()) {
      targets.push_back(ObjectTarget{mem->object, {}});
      if (auto* id = mem->object->As<Identifier>()) {
        seen_names.insert(id->name);
      }
    }
  }
  for (Expression* arg : call->arguments) {
    if (arg->Is<SpreadElement>()) {
      continue;
    }
    if (auto* id = arg->As<Identifier>()) {
      if (id->name == "undefined" || id->name == "eval" ||
          id->name == "arguments") {
        continue;
      }
      const Binding* binding = b.scopes().BindingOf(id);
      JsType ty = binding != nullptr ? b.GuessType(binding) : JsType::kUnknown;
      if (binding != nullptr && IsObjectCandidateType(ty) &&
          seen_names.insert(id->name).second) {
        targets.push_back(ObjectTarget{id, {}});
      }
    } else if (auto* mem = arg->As<MemberExpression>()) {
      const Identifier* root = nullptr;
      if (!mem->computed && !mem->optional && mem->property->Is<Identifier>() &&
          AnalyzeReceiverChain(mem->object, &root)) {
        const bool unbound_root =
            root != nullptr && b.scopes().BindingOf(root) == nullptr;
        if (unbound_root && IsHarnessGlobal(root->name)) {
          continue;
        }
        if ((root == nullptr || (unbound_root && root->name == "globalThis")) &&
            IsHarnessGlobal(FirstChainProperty(mem))) {
          continue;
        }
        targets.push_back(ObjectTarget{mem, {}});
      }
    }
  }
  if (!targets.empty()) {
    return targets;
  }
  for (const Variable& var : b.VisibleAt(call)) {
    if (var.binding == nullptr || var.binding->kind == BindingKind::kImplicit ||
        var.name == "arguments" || IsHarnessGlobal(var.name) ||
        !IsObjectCandidateType(var.type) || var.type == JsType::kUnknown) {
      continue;
    }
    if (seen_names.insert(var.name).second) {
      targets.push_back(ObjectTarget{nullptr, var.name});
    }
  }
  return targets;
}

std::string_view PickTargetPropertyKey(Builder& b, const ObjectTarget& target,
                                       std::string_view forbidden_key,
                                       bool prefer_existing) {
  Random& r = b.rng();
  if (prefer_existing) {
    std::string_view target_name;
    if (target.expr != nullptr) {
      if (auto* id = target.expr->As<Identifier>()) {
        target_name = id->name;
      }
    } else {
      target_name = target.var_name;
    }
    if (!target_name.empty()) {
      const BindingInfo* info = b.InfoOf(target_name);
      if (info != nullptr && !info->keys.empty()) {
        std::vector<std::string_view> keys;
        for (const KnownKey& k : info->keys) {
          if (!IsReservedPropertyKey(k.name) && k.name != forbidden_key) {
            keys.push_back(k.name);
          }
        }
        if (!keys.empty()) {
          return keys[r.Index(keys.size())];
        }
      }
    }
  }
  static constexpr std::string_view kFallbackKeys[] = {"x", "y", "value", "p"};
  for (int i = 0; i < 4; i++) {
    std::string_view cand = kFallbackKeys[r.Index(4)];
    if (cand != forbidden_key) {
      return cand;
    }
  }
  return b.FreshName("p");
}

Statement* OptionalGcStatement(Builder& b) {
  if (!b.rng().Chance(kCallSiteStressGcChance())) {
    return nullptr;
  }
  return mk::ExprStmt(b.tree(), mk::Call(b.tree(), mk::Id(b.tree(), "gc"), {}));
}

bool ApplyReentrantPattern(Builder& b, const CallSite& site,
                           const std::vector<ObjectTarget>& targets,
                           std::string_view forbidden_key,
                           MutationContext& ctx) {
  if (targets.empty()) {
    return false;
  }
  Tree* t = b.tree();
  Random& r = b.rng();
  const ObjectTarget& target = targets[r.Index(targets.size())];
  auto clone_target = [&]() -> Expression* {
    if (target.expr != nullptr) {
      return static_cast<Expression*>(t->Clone(target.expr));
    }
    return mk::Id(t, target.var_name);
  };
  bool do_accessor =
      CanRunInSyncArrow(site.call) && r.Chance(kCallSiteStressAccessorChance());

  std::vector<Statement*> try_body;
  if (do_accessor) {
    std::string_view key = PickTargetPropertyKey(b, target, forbidden_key,
                                                 /*prefer_existing=*/true);
    std::string_view entry_var = b.FreshName("d");
    std::string_view val_var = b.FreshName("v");
    std::string_view new_var = b.FreshName("nv");
    int entry_budget = kCallSiteStressReentrantDepth.AsInt();

    auto build_entry_guard = [&]() -> Statement* {
      return mk::If(t,
                    mk::Binary(t, BinaryOp::kLessThan,
                               mk::Update(t, UpdateOp::kIncrement, false,
                                          mk::Id(t, entry_var)),
                               mk::Num(t, static_cast<double>(entry_budget))),
                    mk::Block(t, {CloneCallStmt(t, site.call)}));
    };
    Expression* getter = mk::Arrow(
        t, {}, {build_entry_guard(), mk::Return(t, mk::Id(t, val_var))});
    Expression* setter = mk::Arrow(
        t, {new_var},
        {mk::ExprStmt(t, mk::Assign(t, mk::Id(t, val_var), mk::Id(t, new_var))),
         build_entry_guard()});
    Expression* desc =
        mk::Object(t, {mk::Prop(t, "configurable", mk::Bool(t, true)),
                       mk::Prop(t, "get", getter), mk::Prop(t, "set", setter)});
    Expression* define_prop =
        mk::Call(t, mk::Member(t, mk::Id(t, "Object"), "defineProperty"),
                 {clone_target(), mk::Str(t, key), desc});

    try_body.push_back(mk::Let(t, entry_var, mk::Num(t, 0)));
    try_body.push_back(
        mk::Let(t, val_var, mk::Index(t, clone_target(), mk::Str(t, key))));
    try_body.push_back(mk::ExprStmt(t, define_prop));
  } else {
    // Adds and optionally deletes a property to transition the object shape.
    std::string_view key = b.FreshName("p");
    if (key == forbidden_key) {
      key = b.FreshName("q");
    }
    try_body.push_back(mk::ExprStmt(
        t, mk::Assign(t, mk::Index(t, clone_target(), mk::Str(t, key)),
                      mk::Num(t, 1.5))));
    if (r.Coin()) {
      try_body.push_back(mk::ExprStmt(
          t, mk::Unary(t, UnaryOp::kDelete,
                       mk::Index(t, clone_target(), mk::Str(t, key)))));
    }
  }
  if (Statement* gc_stmt = OptionalGcStatement(b)) {
    try_body.push_back(gc_stmt);
  }
  Statement* guarded = mk::Try(t, std::move(try_body), b.FreshName("e"), {});
  Builder::Slot slot = ObtainStmtSlot(b, site.stmt);
  if (!slot.valid()) {
    return false;
  }
  b.Insert(slot, guarded);
  ctx.detail = do_accessor ? "call-site re-entrant accessor"
                           : "call-site shape perturbation";
  return true;
}

class CallSiteStressMutator final : public Mutator {
 public:
  // Uses weight 0 by default.
  explicit CallSiteStressMutator(bool natives = true)
      : Mutator("CallSiteStress",
                "applies JIT, GC, stack-limit, or re-entrant stress at call "
                "sites",
                0, false),
        natives_(natives) {}

  void SetNatives(bool natives) override { natives_ = natives; }

  bool Mutate(Builder& b, MutationContext& ctx) override {
    std::vector<CallSite> sites = CollectCallSites(b);
    if (sites.empty()) {
      return false;
    }
    Random& r = b.rng();
    std::vector<double> weights;
    weights.reserve(sites.size());
    for (const CallSite& site : sites) {
      weights.push_back(site.score);
    }
    int attempts = std::min(kCallSiteStressMaxAttempts.AsInt(),
                            static_cast<int>(sites.size()));
    for (int i = 0; i < attempts; i++) {
      size_t idx = r.Weighted(weights);
      if (idx >= sites.size() || weights[idx] <= 0.0) {
        break;
      }
      weights[idx] = 0.0;
      if (ApplyAtSite(b, sites[idx], ctx)) {
        return true;
      }
    }
    return false;
  }

 private:
  enum class Pattern {
    kTierUp,
    kAllocTimeout,
    kDeoptRetier,
    kStackLimit,
    kReentrant,
  };

  static bool HasIntermediateBoundary(Builder& b, Node* call, Statement* stmt) {
    Node* cur = b.ParentOf(call);
    while (cur != nullptr && cur != stmt) {
      if (cur->IsFunction() || cur->IsClass()) {
        return true;
      }
      cur = b.ParentOf(cur);
    }
    return cur != stmt;
  }

  static bool CheckEnclosingLoops(Builder& b, Node* call, int* depth_out) {
    int depth = 0;
    double max_bound = kCallSiteStressMaxLoopBound();
    int max_depth = kCallSiteStressMaxLoopDepth.AsInt();
    Node* cur = b.ParentOf(call);
    while (cur != nullptr) {
      if (cur->IsFunction() || cur->IsClass()) {
        break;
      }
      if (IsLoopNode(cur)) {
        if (IsHugeOrInfiniteLoop(cur, max_bound)) {
          return false;
        }
        depth++;
        if (depth > max_depth) {
          return false;
        }
      }
      cur = b.ParentOf(cur);
    }
    *depth_out = depth;
    return true;
  }

  static bool IsEligibleStatement(Builder& b, Statement* stmt) {
    if (stmt == nullptr || IsDirective(stmt)) {
      return false;
    }
    if (!IsInsideFocus(b, b.ParentOf(stmt))) {
      return false;
    }
    bool kind_ok = false;
    if (auto* expr = stmt->As<ExpressionStatement>()) {
      kind_ok = !expr->is_directive;
    } else if (auto* decl = stmt->As<VariableDeclaration>()) {
      kind_ok = decl->declaration_kind != VariableKind::kUsing &&
                decl->declaration_kind != VariableKind::kAwaitUsing &&
                decl->declarations.size() == 1 &&
                decl->declarations[0]->init != nullptr;
    } else if (auto* ret = stmt->As<ReturnStatement>()) {
      kind_ok = ret->argument != nullptr;
    }
    if (!kind_ok) {
      return false;
    }
    return b.SlotOf(stmt).valid() || IsSingleStatementControlBody(b, stmt);
  }

  std::vector<CallSite> CollectCallSites(Builder& b) const {
    std::vector<CallSite> sites;
    std::vector<Node*> calls =
        b.Collect([&](Node* n) { return n->Is<CallExpression>(); });
    for (Node* node : calls) {
      auto* call = node->Cast<CallExpression>();
      if (call->optional || !IsSimpleCallee(b, call->callee) ||
          IsInsideNearStackHelper(b, call)) {
        continue;
      }
      Node* call_parent = b.ParentOf(call);
      if (call_parent != nullptr && call_parent->Is<ChainExpression>()) {
        continue;
      }
      if (IsSelfRecursiveCall(b, call)) {
        continue;
      }
      Statement* stmt = b.EnclosingStatement(call);
      if (!IsEligibleStatement(b, stmt) ||
          HasIntermediateBoundary(b, call, stmt)) {
        continue;
      }
      int loop_depth = 0;
      if (!CheckEnclosingLoops(b, call, &loop_depth)) {
        continue;
      }
      CallSite site;
      site.call = call;
      site.stmt = stmt;
      site.loop_depth = loop_depth;
      site.in_function = b.EnclosingFunction(call) != nullptr;

      std::string_view unused_forbidden;
      std::vector<ObjectTarget> obj_targets =
          CollectObjectTargets(b, call, &unused_forbidden);
      std::vector<Pattern> unused_patterns;
      std::vector<double> unused_weights;
      CollectEligiblePatterns(b, site, !obj_targets.empty(), &unused_patterns,
                              &unused_weights);
      if (unused_patterns.empty()) {
        continue;
      }

      double score = 1.0;
      if (site.in_function) {
        score *= 2.0;
      }
      if (auto* id = call->callee->As<Identifier>()) {
        const Binding* binding = b.scopes().BindingOf(id);
        if (binding != nullptr) {
          JsType ty = b.GuessType(binding);
          site.known_function_binding = (ty == JsType::kFunction);
          score *= site.known_function_binding ? 4.0 : 2.0;
        }
      } else {
        score *= 2.0;
      }
      site.score = score;
      sites.push_back(site);
    }
    return sites;
  }

  void CollectEligiblePatterns(Builder& b, const CallSite& site,
                               bool has_object_target,
                               std::vector<Pattern>* patterns,
                               std::vector<double>* pweights) const {
    auto push = [&](Pattern p, double w) {
      if (w > 0.0) {
        patterns->push_back(p);
        pweights->push_back(w);
      }
    };
    if (natives_) {
      push(Pattern::kTierUp, kCallSiteStressTierUpWeight());
      if (CanWrapInFinallyBlock(b, site.stmt)) {
        push(Pattern::kAllocTimeout, kCallSiteStressAllocTimeoutWeight());
      }
      push(Pattern::kDeoptRetier, kCallSiteStressDeoptRetierWeight());
    }
    const bool focus_allows_helper = b.focus_subtree() == nullptr ||
                                     b.focus_subtree() == b.root() ||
                                     HasTopLevelNearStackHelper(b);
    if (focus_allows_helper && !b.root()->body.empty() &&
        b.scopes().root()->Lookup("RangeError") == nullptr &&
        !IsInsideWorkerCallback(b, site.call) && CanRunInSyncArrow(site.call)) {
      push(Pattern::kStackLimit, kCallSiteStressStackLimitWeight());
    }
    if (has_object_target) {
      push(Pattern::kReentrant, kCallSiteStressReentrantWeight());
    }
  }

  bool ApplyAtSite(Builder& b, const CallSite& site,
                   MutationContext& ctx) const {
    std::string_view forbidden_key;
    std::vector<ObjectTarget> obj_targets =
        CollectObjectTargets(b, site.call, &forbidden_key);

    std::vector<Pattern> patterns;
    std::vector<double> pweights;
    CollectEligiblePatterns(b, site, !obj_targets.empty(), &patterns,
                            &pweights);
    if (patterns.empty()) {
      return false;
    }

    Random& r = b.rng();
    Pattern picked = patterns[r.Weighted(pweights)];
    bool guard = ShouldGuardCallPrefix(b, site);
    Tree* t = b.tree();

    switch (picked) {
      case Pattern::kTierUp: {
        bool used_maglev = false;
        std::vector<Statement*> prefix =
            BuildTierUpPrefix(b, site, guard, &used_maglev);
        Builder::Slot slot = ObtainStmtSlot(b, site.stmt);
        if (!slot.valid()) {
          return false;
        }
        b.InsertAll(slot, prefix);
        ctx.detail = used_maglev ? "call-site tier-up (Maglev)"
                                 : "call-site tier-up (TurboFan)";
        return true;
      }
      case Pattern::kAllocTimeout: {
        bool used_maglev = false;
        std::vector<Statement*> prefix =
            BuildTierUpPrefix(b, site, guard, &used_maglev);
        Builder::Slot slot = ObtainStmtSlot(b, site.stmt);
        if (!slot.valid()) {
          return false;
        }
        Statement* inner_stmt = site.stmt;
        if (!b.EscapingBindings({site.stmt}, /*block_scoped_only=*/true)
                 .empty()) {
          auto* decl = site.stmt->Cast<VariableDeclaration>();
          std::string_view name =
              decl->declarations[0]->target->Cast<Identifier>()->name;
          VariableDeclaration* decl_only = mk::Let(t, name, nullptr);
          prefix.push_back(decl_only);
          inner_stmt = mk::ExprStmt(
              t, mk::Assign(t, mk::Id(t, name), decl->declarations[0]->init));
        }
        int64_t countdown = r.Range(
            1, std::max<int64_t>(1, kCallSiteStressAllocTimeoutMax.AsInt()));
        TryStatement* try_stmt =
            BuildAllocTimeoutTry(b, inner_stmt, countdown, r.Coin());
        prefix.push_back(try_stmt);
        slot.list->erase(slot.list->begin() +
                         static_cast<std::ptrdiff_t>(slot.index));
        b.Invalidate();
        b.InsertAll(slot, prefix);
        ctx.detail = StringPrintf("call-site alloc timeout (%lld)",
                                  static_cast<long long>(countdown));
        return true;
      }
      case Pattern::kDeoptRetier: {
        bool used_baseline = false;
        std::vector<Statement*> prefix =
            BuildDeoptRetierPrefix(b, site, guard, &used_baseline);
        Builder::Slot slot = ObtainStmtSlot(b, site.stmt);
        if (!slot.valid()) {
          return false;
        }
        b.InsertAll(slot, prefix);
        ctx.detail = used_baseline ? "call-site %CompileBaseline"
                                   : "call-site %DeoptimizeFunction";
        return true;
      }
      case Pattern::kStackLimit: {
        CallExpression* call_clone =
            static_cast<CallExpression*>(t->Clone(site.call));
        Expression* thunk = mk::ArrowExpr(t, {}, call_clone);
        double depth = kCallSiteStressStackDepth();
        bool already_exists = false;
        std::string_view helper = ChooseNearStackHelperName(b, &already_exists);
        CallExpression* wrapped =
            mk::Call(t, mk::Id(t, helper), {thunk, mk::Num(t, depth)});
        if (!b.Replace(site.call, wrapped)) {
          return false;
        }
        if (!already_exists) {
          InsertNearStackHelper(b, helper);
        }
        ctx.detail = !already_exists
                         ? "call-site near-stack-limit (inserted helper)"
                         : "call-site near-stack-limit";
        return true;
      }
      case Pattern::kReentrant:
        return ApplyReentrantPattern(b, site, obj_targets, forbidden_key, ctx);
    }
    return false;
  }

  bool natives_ = true;
};

}  // namespace

// --- MutatorSet
// ------------------------------------------------------------------------------

// static
std::unique_ptr<MutatorSet> MutatorSet::CreateDefault() {
  auto set = std::make_unique<MutatorSet>();
  auto add = [&](std::unique_ptr<Mutator> m) {
    set->candidates_.push_back(m.get());
    set->mutators_.push_back(std::move(m));
  };
  // Fuzzilli: CodeGenMutator, weight 2, 1-3 insertion points per
  // application, each with about five instructions' worth of new code
  // (defaultCodeGenerationAmount) = 1-3 generator outputs here.
  add(std::make_unique<InsertMutator>("CodeGen",
                                      "inserts freshly generated statements", 2,
                                      std::vector<std::string>{}, false,
                                      /*min_count=*/1,
                                      /*extra_count=*/2,
                                      /*at_top=*/false,
                                      /*max_simultaneous=*/3));
  add(std::make_unique<ExploreMutator>());
  add(std::make_unique<ProbingMutator>());
  add(std::make_unique<FixupMutator>());
  // Weight 0: never picked at random; the worker applies it to the parent of
  // every mutation chain so that the mutators have fresh values to work with
  // (10-15 values, like Fuzzilli's buildPrefix()). Only generators that
  // cannot throw, in a context that makes CodeGen produce values that cannot
  // throw: an exception in the prefix would fail every mutant of the chain.
  std::vector<std::string> prefix_generators = {
      "IntegerDecl", "FloatDecl",      "StringDecl", "BooleanDecl",
      "BigIntDecl",  "RegExpDecl",     "SymbolDecl", "ArrayDecl",
      "ObjectDecl",  "TypedArrayDecl", "ArrowDecl",  "TemplateDecl"};
  for (const std::string& name : CannotThrowValueGenerators()) {
    prefix_generators.push_back(name);
  }
  add(std::make_unique<InsertMutator>(
      "Prefix",
      "fresh value declarations at the top of a mutation chain's parent", 0,
      std::move(prefix_generators), false,
      /*min_count=*/10,
      /*extra_count=*/5,
      /*at_top=*/true,
      /*max_simultaneous=*/1,
      /*cannot_throw=*/true));
  add(std::make_unique<SpliceMutator>());
  add(std::make_unique<CombineMutator>());
  add(std::make_unique<InputMutator>());
  add(std::make_unique<OperationMutator>());
  add(std::make_unique<ExpressionMutator>());
  add(std::make_unique<PropertyAccessorMutator>());
  add(std::make_unique<CallMutator>());
  add(std::make_unique<RemoveMutator>());
  add(std::make_unique<WrapMutator>());
  add(std::make_unique<DuplicateMutator>());
  add(std::make_unique<ReorderMutator>());
  // No Fuzzilli counterpart as a mutator (Fuzzilli reaches the JIT through
  // its code generators, which CodeGen also has): weight 0 by default.
  add(std::make_unique<InsertMutator>(
      "JitStress",
      "inserts JIT tiering/deopt/GC idioms around existing functions", 0,
      std::vector<std::string>{"JitOptimize", "JitBaseline", "Deopt", "OsrLoop",
                               "NativeCall", "Gc", "ElementsKind",
                               "ShapeChange"},
      true));
  add(std::make_unique<JitCompoundMutator>());
  add(std::make_unique<CallSiteStressMutator>());
  std::string error;
  if (!set->SetSelector(
          std::make_unique<WeightedSelector<Mutator>>(
              "mutator", std::vector<std::pair<std::string, double>>{}),
          &error)) {
    REDLINE_ERROR("mutators", "default mutator strategy: %s", error.c_str());
    return nullptr;
  }
  return set;
}

bool MutatorSet::ConfigurePrefix(int min_values, int extra_values) {
  Mutator* prefix = Find("Prefix");
  return prefix != nullptr && prefix->SetInsertCounts(min_values, extra_values);
}

bool MutatorSet::SetSelector(std::unique_ptr<Selector<Mutator>> selector,
                             std::string* error) {
  if (!selector->Initialize(candidates_, error)) {
    return false;
  }
  selector_ = std::move(selector);
  return true;
}

Mutator* MutatorSet::Find(std::string_view name) const {
  for (const auto& m : mutators_) {
    if (m->name() == name) {
      return m.get();
    }
  }
  return nullptr;
}

std::vector<std::string> MutatorSet::Names() const {
  std::vector<std::string> names;
  names.reserve(mutators_.size());
  for (const auto& m : mutators_) {
    names.push_back(m->name());
  }
  return names;
}

Mutator* MutatorSet::Pick(Random& rng, bool has_partner) const {
  size_t index = PickWithMask<Mutator>(
      *selector_, candidates_, rng,
      [has_partner](const Mutator& m) {
        return has_partner || !m.needs_partner();
      },
      ActiveMutatorMask());
  return index == Selector<Mutator>::kNoChoice ? nullptr : candidates_[index];
}

const std::vector<std::string>& SwarmStructuralMutators() {
  static const std::vector<std::string>* names =
      new std::vector<std::string>{"Wrap", "Duplicate", "Reorder", "JitStress"};
  return *names;
}

// --- selection strategies for mutators -------------------------------------
//
// To add one: derive from Selector<Mutator>, then add a Register() line
// below. Parameters are declared in the Entry so that the configuration is
// validated before the fuzzer starts.

template <>
void RegisterBuiltinSelectors<Mutator>(SelectorRegistry<Mutator>& registry) {
  registry.Register({std::string(WeightedSelector<Mutator>::kName),
                     "one weighted random draw among the eligible mutators",
                     WeightedSelector<Mutator>::Specs(),
                     [](const JsonValue& params, std::string* error)
                         -> std::unique_ptr<Selector<Mutator>> {
                       std::vector<std::pair<std::string, double>> overrides;
                       if (!ReadWeightOverrides(params, &overrides, error)) {
                         return nullptr;
                       }
                       // Names are checked against the mutators in
                       // Initialize().
                       return std::make_unique<WeightedSelector<Mutator>>(
                           "mutator", std::move(overrides));
                     }});
  registry.SetDefault(WeightedSelector<Mutator>::kName);
  registry.Register({std::string(AdaptiveSelector<Mutator>::kName),
                     "weighted draw whose weights follow each mutator's recent "
                     "yield (new corpus programs per execution)",
                     AdaptiveSelector<Mutator>::Specs(),
                     [](const JsonValue& params, std::string* error)
                         -> std::unique_ptr<Selector<Mutator>> {
                       std::vector<std::pair<std::string, double>> overrides;
                       if (!ReadWeightOverrides(params, &overrides, error)) {
                         return nullptr;
                       }
                       AdaptiveSelector<Mutator>::Settings p =
                           AdaptiveSelector<Mutator>::ReadParams(params);
                       if (!AdaptiveSelector<Mutator>::Validate(p, error)) {
                         return nullptr;
                       }
                       return std::make_unique<AdaptiveSelector<Mutator>>(
                           "mutator", std::move(overrides), p);
                     }});
}

std::vector<const Component*> MutatorSet::Components() const {
  std::vector<const Component*> components;
  components.reserve(mutators_.size());
  for (const auto& m : mutators_) {
    components.push_back(m.get());
  }
  return components;
}

namespace {

bool IsLoopNodeKind(NodeKind kind) {
  return kind == NodeKind::kForStatement || kind == NodeKind::kWhileStatement ||
         kind == NodeKind::kDoWhileStatement ||
         kind == NodeKind::kForInStatement || kind == NodeKind::kForOfStatement;
}

struct FocusScanState {
  struct RawCandidate {
    uint32_t body_id = 0;
    std::string_view name;
    bool is_method = false;
    bool is_top_level_decl = false;
    bool is_generator = false;
    bool body_has_loop = false;
    bool body_has_osr = false;
  };

  fast::Program* root = nullptr;
  std::vector<RawCandidate> candidates;
  std::vector<size_t> fn_stack;
  absl::flat_hash_set<std::string_view> prepared_or_opt_names;
  absl::flat_hash_set<std::string_view> tiered_up_names;
  absl::flat_hash_set<std::string_view> loop_called_names;
  absl::flat_hash_set<std::string_view> loop_called_methods;

  void Scan(Node* node, Node* parent, int loop_depth) {
    if (node == nullptr) {
      return;
    }
    if (auto* native = node->As<NativeCallExpression>()) {
      if (native->name == "PrepareFunctionForOptimization" ||
          native->name == "OptimizeFunctionOnNextCall" ||
          native->name == "OptimizeMaglevOnNextCall") {
        if (!native->arguments.empty()) {
          if (auto* id = native->arguments[0]->As<Identifier>()) {
            prepared_or_opt_names.insert(id->name);
            if (native->name != "PrepareFunctionForOptimization") {
              tiered_up_names.insert(id->name);
            }
          }
        }
      } else if (native->name == "OptimizeOsr" ||
                 native->name == "BaselineOsr") {
        if (!fn_stack.empty()) {
          candidates[fn_stack.back()].body_has_osr = true;
        }
      }
    }
    if (loop_depth > 0) {
      if (auto* call = node->As<CallExpression>()) {
        if (auto* id = call->callee->As<Identifier>()) {
          loop_called_names.insert(id->name);
        } else if (auto* mem = call->callee->As<MemberExpression>();
                   mem != nullptr && !mem->computed) {
          if (auto* prop = mem->property->As<Identifier>()) {
            loop_called_methods.insert(prop->name);
          }
        }
      } else if (auto* construct = node->As<NewExpression>()) {
        if (auto* id = construct->callee->As<Identifier>()) {
          loop_called_names.insert(id->name);
        }
      }
    }
    if (FunctionData* fn = AsFunctionData(node)) {
      if (fn->body != nullptr) {
        RawCandidate cand;
        cand.body_id = fn->body->id;
        cand.is_generator = fn->is_generator;
        if (node->Is<FunctionDeclaration>()) {
          if (fn->name != nullptr) {
            cand.name = fn->name->name;
            cand.is_top_level_decl = (parent == root);
          }
        } else if (parent != nullptr && parent->Is<MethodDefinition>() &&
                   parent->Cast<MethodDefinition>()->value == node) {
          auto* m = parent->Cast<MethodDefinition>();
          cand.is_method = true;
          if (!m->computed && m->key != nullptr) {
            if (auto* id = m->key->As<Identifier>()) {
              cand.name = id->name;
            }
          }
        } else if (parent != nullptr && parent->Is<Property>() &&
                   parent->Cast<Property>()->value == node &&
                   (parent->Cast<Property>()->method ||
                    parent->Cast<Property>()->property_kind !=
                        PropertyKind::kInit)) {
          auto* p = parent->Cast<Property>();
          cand.is_method = true;
          if (!p->computed && p->key != nullptr) {
            if (auto* id = p->key->As<Identifier>()) {
              cand.name = id->name;
            }
          }
        } else if (parent != nullptr && parent->Is<VariableDeclarator>() &&
                   parent->Cast<VariableDeclarator>()->init == node &&
                   parent->Cast<VariableDeclarator>()->target != nullptr &&
                   parent->Cast<VariableDeclarator>()
                       ->target->Is<Identifier>()) {
          cand.name = parent->Cast<VariableDeclarator>()
                          ->target->Cast<Identifier>()
                          ->name;
        } else if (parent != nullptr && parent->Is<AssignmentExpression>() &&
                   parent->Cast<AssignmentExpression>()->right == node &&
                   parent->Cast<AssignmentExpression>()->left != nullptr &&
                   parent->Cast<AssignmentExpression>()
                       ->left->Is<Identifier>()) {
          cand.name = parent->Cast<AssignmentExpression>()
                          ->left->Cast<Identifier>()
                          ->name;
        } else if (fn->name != nullptr) {
          cand.name = fn->name->name;
        }
        const size_t idx = candidates.size();
        candidates.push_back(cand);
        for (Expression* param : fn->params) {
          Scan(param, node, 0);
        }
        fn_stack.push_back(idx);
        Scan(fn->body, node, 0);
        fn_stack.pop_back();
        return;
      }
      // Concise arrow function: scan children so nested block functions appear.
      ForEachChild(node, [&](Node* child) { Scan(child, node, 0); });
      return;
    }
    int child_loop_depth = loop_depth;
    if (IsLoopNodeKind(node->kind)) {
      child_loop_depth++;
      if (!fn_stack.empty()) {
        candidates[fn_stack.back()].body_has_loop = true;
      }
    }
    ForEachChild(node,
                 [&](Node* child) { Scan(child, node, child_loop_depth); });
  }
};

}  // namespace

bool PickFocusTarget(const fast::Tree& tree, Random& rng, FocusTarget* out) {
  if (tree.root() == nullptr) {
    return false;
  }
  FocusScanState state;
  state.root = tree.root();
  state.Scan(tree.root(), nullptr, 0);
  if (state.candidates.empty()) {
    return false;
  }
  std::vector<FocusTarget> hot;
  std::vector<FocusTarget> all;
  all.reserve(state.candidates.size());
  for (const FocusScanState::RawCandidate& c : state.candidates) {
    FocusTarget target;
    target.body_id = c.body_id;
    target.name = std::string(c.name);
    target.is_top_level_decl = c.is_top_level_decl;
    target.is_generator = c.is_generator;
    const bool named = !c.name.empty();
    const bool in_opt =
        named && !c.is_method && state.prepared_or_opt_names.count(c.name) > 0;
    const bool tiered =
        named && !c.is_method && state.tiered_up_names.count(c.name) > 0;
    const bool in_loop_call =
        named && (c.is_method ? state.loop_called_methods.count(c.name) > 0
                              : state.loop_called_names.count(c.name) > 0);
    target.is_hot = c.body_has_osr || c.body_has_loop || in_opt || in_loop_call;
    target.has_tier_up = c.body_has_osr || tiered || in_loop_call;
    if (target.is_hot) {
      hot.push_back(target);
    }
    all.push_back(std::move(target));
  }
  const std::vector<FocusTarget>& pool = hot.empty() ? all : hot;
  *out = pool[rng.Index(pool.size())];
  return true;
}

std::unique_ptr<fast::Tree> ApplyFocusTierUp(const redline::Program& parent,
                                             const FocusTarget& target,
                                             Random& rng,
                                             const Environment& env,
                                             const GeneratorSet& generators,
                                             std::string* detail) {
  Generator* jit_opt = generators.Find("JitOptimize");
  if (jit_opt == nullptr || generators.WeightOf(jit_opt) <= 0.0) {
    return nullptr;
  }
  std::unique_ptr<Tree> tree = parent.CopyTree();
  Builder builder(tree.get(), &rng, &env);
  Tree* t = builder.tree();
  // Append warm-up and optimization calls for a top-level function declaration.
  if (target.is_top_level_decl && !target.name.empty()) {
    Builder::Slot end_slot{&t->root()->body, t->root()->body.size(), t->root()};
    GenContext gen = MakeGenContext(builder, end_slot, generators, env);
    bool visible_as_fn = false;
    for (const Variable& v : gen.visible) {
      if (v.name == target.name && v.type == JsType::kFunction) {
        visible_as_fn = true;
        break;
      }
    }
    if (visible_as_fn) {
      CodeGen cg(gen);
      std::string_view name = t->Intern(target.name);
      CallExpression* template_call = cg.CallWithArguments(mk::Id(t, name));
      auto make_call = [&]() -> Statement* {
        std::vector<Expression*> args;
        args.reserve(template_call->arguments.size());
        for (const Expression* arg : template_call->arguments) {
          args.push_back(t->Clone(arg));
        }
        CallExpression* call = mk::Call(t, mk::Id(t, name), std::move(args));
        if (target.is_generator) {
          return cg.Stmt(mk::CallMethod(t, call, "next", {}));
        }
        return cg.Stmt(call);
      };
      std::vector<Statement*> stmts;
      stmts.push_back(cg.Stmt(
          mk::Native(t, "PrepareFunctionForOptimization", {mk::Id(t, name)})));
      stmts.push_back(make_call());
      stmts.push_back(make_call());
      const char* tier = rng.Coin() ? "OptimizeFunctionOnNextCall"
                                    : "OptimizeMaglevOnNextCall";
      stmts.push_back(cg.Stmt(mk::Native(t, tier, {mk::Id(t, name)})));
      stmts.push_back(make_call());
      builder.InsertAll(end_slot, stmts);
      jit_opt->stats().invocations++;
      jit_opt->stats().produced++;
      NoteGeneratorUsed(jit_opt);
      if (detail != nullptr) {
        *detail = "focus tier-up " + target.name;
      }
      return tree;
    }
  }
  // Run `JitOptimize` once for other function kinds outside the target body.
  auto inside_target_body = [&](Node* node) {
    for (Node* n = node; n != nullptr; n = builder.ParentOf(n)) {
      if (n->id == target.body_id) {
        return true;
      }
    }
    return false;
  };
  Builder::Slot slot;
  for (int attempt = 0; attempt < 6; attempt++) {
    Builder::Slot candidate = builder.RandomInsertionSlot();
    if (!candidate.valid()) {
      return nullptr;
    }
    if (inside_target_body(candidate.owner)) {
      continue;
    }
    slot = candidate;
    if (!Builder::OfType(builder.VisibleAtSlot(slot), JsType::kFunction)
             .empty()) {
      break;
    }
  }
  if (!slot.valid()) {
    slot = Builder::Slot{&t->root()->body, t->root()->body.size(), t->root()};
  }
  GenContext gen = MakeGenContext(builder, slot, generators, env);
  if (!jit_opt->Applicable(gen)) {
    return nullptr;
  }
  std::vector<Statement*> out;
  gen.out = &out;
  jit_opt->stats().invocations++;
  if (!jit_opt->Generate(gen, &out) || out.empty()) {
    return nullptr;
  }
  jit_opt->stats().produced++;
  NoteGeneratorUsed(jit_opt);
  builder.InsertAll(slot, out);
  if (detail != nullptr) {
    *detail = "focus JitOptimize";
  }
  return tree;
}

std::unique_ptr<fast::Tree> ApplyMutation(
    Mutator& mutator, const redline::Program& parent, const ProgramPtr& partner,
    Random& rng, const Environment& env, const GeneratorSet& generators,
    int rounds, std::string* detail, std::vector<ExploreProbe>* probes,
    std::optional<uint32_t> focus_node_id) {
  std::unique_ptr<Tree> tree = parent.CopyTree();
  Builder builder(tree.get(), &rng, &env);
  Node* focus_node =
      focus_node_id.has_value() ? tree->FindNode(*focus_node_id) : nullptr;
  if (focus_node != nullptr) {
    builder.SetFocusSubtree(focus_node);
  }
  MutationContext ctx;
  ctx.partner = partner;
  ctx.env = &env;
  ctx.generators = &generators;
  int applied = 0;
  std::string all;
  // Runtime-assisted mutators instrument once; their probes are completed
  // by the worker.
  if (mutator.runtime_assisted()) {
    rounds = 1;
  }
  auto run_rounds = [&](bool count_invocations) {
    for (int i = 0; i < rounds; i++) {
      ctx.detail.clear();
      if (count_invocations) {
        mutator.stats().invocations++;
      }
      if (mutator.Mutate(builder, ctx)) {
        applied++;
        if (!all.empty()) {
          all += "; ";
        }
        all += ctx.detail;
      }
    }
  };
  run_rounds(/*count_invocations=*/true);
  if (applied == 0 && focus_node != nullptr) {
    // Retry on the full tree when the focused subtree has no valid candidate.
    builder.SetFocusSubtree(nullptr);
    run_rounds(/*count_invocations=*/false);
  }
  if (applied == 0) {
    return nullptr;
  }
  mutator.stats().produced++;
  if (detail != nullptr) {
    *detail = all;
  }
  if (probes != nullptr) {
    *probes = std::move(ctx.probes);
  }
  return tree;
}

}  // namespace redline
