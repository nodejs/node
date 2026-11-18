// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_MEMBERS_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_MEMBERS_H_

#include <optional>
#include <string_view>
#include <vector>

#include "experimental/redline/generators/codegen.h"

// Members of classes and object literals, and the accesses to them. This
// file has the shared pools of member names and the members of ClassDecl.
// It also has the known keys of values and the member access generators.
namespace redline {

// --- member names ------------------------------------------------------------
//
// Small pools of member names, after Fuzzilli's custom names
// (Environment/JavaScriptEnvironment.swift:307-310). Classes, object literals
// and the accesses draw from the same pools. Thus accesses often find a real
// member.

// A property name: a-f, "!" or "42".
std::string_view RandomMemberPropertyName(Random& r);
// A method name: m, n, o, "?", "67", valueOf or toString.
std::string_view RandomMemberMethodName(Random& r);
// The name of a private field or accessor, without the '#': a-f.
std::string_view RandomPrivateFieldName(Random& r);
// The name of a private method, without the '#': m, n, o or p.
std::string_view RandomPrivateMethodName(Random& r);
// The key node for the member name `name`: an identifier, a numeric literal
// or a string literal.
fast::Expression* MemberKey(fast::Tree* t, std::string_view name);

// --- class bodies ------------------------------------------------------------

// The settings of ClassDecl that its member list uses.
struct ClassMemberSettings {
  int count = 1;                 // number of members
  bool has_super_class = false;  // `super.x` reaches a superclass
  double static_chance = 0;
  double method_async_chance = 0;
  double method_generator_chance = 0;
  int method_max_params = 0;
  int method_max_statements = 1;
  double method_this_store_chance = 0;
  double super_call_chance = 0;
};

// The plan of the member list of a new class: its kinds and keys. The
// private names are known before any body exists, so that the constructor,
// the methods and the static block can all use them.
struct ClassMemberPlan {
  enum class Kind { kField, kMethod, kGetter, kSetter };
  enum class KeyKind { kName, kPrivate, kComputed };
  struct Member {
    Kind kind = Kind::kField;
    KeyKind key_kind = KeyKind::kName;
    bool is_static = false;
    std::string_view name;  // for kName and kPrivate keys
  };
  std::vector<Member> members;
  // The private names that the class declares.
  std::vector<PrivateName> private_names;
};

ClassMemberPlan PlanClassMembers(GenContext& ctx, CodeGen& cg,
                                 const ClassMemberSettings& settings);
// Appends the members of `plan` to `members`. ctx.syntax.private_names must
// contain the private names of the plan.
void BuildClassMembers(GenContext& ctx, CodeGen& cg,
                       const ClassMemberSettings& settings,
                       const ClassMemberPlan& plan,
                       std::vector<fast::Node*>* members);

// Adds the private names of a class to ctx.syntax.private_names while the
// class body is generated, and restores the previous list at the end.
class ScopedPrivateNames {
 public:
  ScopedPrivateNames(GenContext& ctx, const std::vector<PrivateName>& names);
  ~ScopedPrivateNames();
  ScopedPrivateNames(const ScopedPrivateNames&) = delete;
  ScopedPrivateNames& operator=(const ScopedPrivateNames&) = delete;
  // Restores the previous list before the end of the scope.
  void Restore();

 private:
  GenContext& ctx_;
  std::vector<PrivateName> saved_;
  bool restored_ = false;
};

// --- known keys --------------------------------------------------------------
//
// Generators record the keys of the values that they make (see
// BindingInfo::keys). Property() and MethodCall() then use real members.

// Records the keys of the value that `init` makes for the new variable
// `name`. The value is an object literal, a class expression, or
// `new C(...)` of a class with known keys.
void RecordDeclaredKeys(GenContext& ctx, std::string_view name,
                        const fast::Expression* init);
// Records the keys of the class that `statement` declares. The statement is
// a class declaration, or a const bound to a class expression. The keys that
// the class inherits from a known superclass are recorded too.
void RecordClassKeys(GenContext& ctx, std::string_view name,
                     const fast::Statement* statement);
// Records the key and the value type of the store `target = value` when
// `target` is `x.p` or `x["p"]` on a variable `x`.
void RecordStore(GenContext& ctx, const fast::Expression* target,
                 const fast::Expression* value);
// A random own key of the value of the variable that `object` names, or
// nothing. With `methods_only`, only a method.
std::optional<KnownKey> RandomKnownKey(GenContext& ctx,
                                       const fast::Expression* object,
                                       bool methods_only);
// The known key that the member expression `target` reads or writes, or
// nothing.
std::optional<KnownKey> KnownKeyOf(GenContext& ctx,
                                   const fast::Expression* target);

// --- object literal members --------------------------------------------------
//
// Fuzzilli makes object literal members with 12 generators
// (CodeGen/CodeGenerators.swift:861-1078). CodeGen::ObjectLiteral uses these.

// The settings of CodeGen::ObjectLiteral that its methods use.
struct LiteralMemberSettings {
  int depth = 0;                 // nesting depth of the literal
  double getter_chance = 0;      // a getter rather than a method
  int max_extra_statements = 0;  // body statements beyond the first one
};

// A method, getter or setter of an object literal. Its key is `name` or a
// computed key. A method has parameters and can be async, a generator or
// both. A setter stores its argument.
fast::Node* LiteralMethod(GenContext& ctx, CodeGen& cg, std::string_view name,
                          const LiteralMemberSettings& settings);
// `__proto__: v` with an object (or null) as v.
fast::Node* LiteralPrototype(GenContext& ctx, CodeGen& cg, int depth);
// The key node of an object literal property named `name`: a plain key, or
// a computed well-known symbol for "Symbol.x".
fast::Expression* LiteralKey(fast::Tree* t, std::string_view name,
                             bool* computed);

// --- generators --------------------------------------------------------------
//
// GeneratorSet::CreateDefault() registers these.

// `o.#x`, `o.#x = v`, `o.#x op= v`, `o.#m(...)` and `#x in o`, with the
// private names of the enclosing classes.
bool GenPrivateAccess(GenContext& ctx, CodeGen& cg,
                      std::vector<fast::Statement*>* out);
// `super.p`, `super[k]`, `super.p = v`, `super[k] = v`, `super.p op= v` and
// `super.m(...)` in a method with a home object.
bool GenSuperAccess(GenContext& ctx, CodeGen& cg,
                    std::vector<fast::Statement*>* out);
// Appends `super.m(...)` to `out`. A method that every object has cannot
// throw. Another method name of the pool is called in a try block.
void SuperMethodCall(GenContext& ctx, CodeGen& cg,
                     std::vector<fast::Statement*>* out);

// Member generators (kClassSlot, kLiteralSlot) append one new member (or an
// accessor pair) for ctx.member_owner to ctx.member_out. Fuzzilli's
// CodeGenMutator also inserts code into class bodies and object literals
// (Mutators/CodeGenMutator.swift:31-78).
//
// A field, method, getter or setter for an existing class body.
bool GenClassMember(GenContext& ctx, CodeGen& cg,
                    std::vector<fast::Statement*>* out);
// A property, method, accessor, spread or `__proto__` entry for an existing
// object literal.
bool GenLiteralMember(GenContext& ctx, CodeGen& cg,
                      std::vector<fast::Statement*>* out);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_MEMBERS_H_
