// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "experimental/redline/base/log.h"
#include "experimental/redline/core/adaptive_selector.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/core/weighted_selector.h"
#include "experimental/redline/generators/codegen.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/generators/wasm_module.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

bool Generator::Applicable(const GenContext& ctx) const {
  const Builder::Context& s = ctx.syntax;
  if ((requirements_ & kNeedsFunction) &&
      (!s.in_function || s.in_static_block || s.in_class_field)) {
    return false;
  }
  if ((requirements_ & kNeedsAsync) && (!s.in_async || s.in_static_block)) {
    return false;
  }
  if ((requirements_ & kNeedsGenerator) && !s.in_generator) {
    return false;
  }
  if ((requirements_ & kNeedsLoop) && !s.in_loop) {
    return false;
  }
  if ((requirements_ & kNeedsBreak) && !s.break_allowed()) {
    return false;
  }
  if ((requirements_ & kNeedsSloppy) && s.strict) {
    return false;
  }
  if ((requirements_ & kNeedsArguments) && !s.has_arguments) {
    return false;
  }
  if ((requirements_ & kNeedsThis) && !s.has_this) {
    return false;
  }
  if ((requirements_ & kRecursive) && ctx.depth >= ctx.max_depth) {
    return false;
  }
  return true;
}

namespace {

using Statements = std::vector<Statement*>;
using GenFn = bool (*)(GenContext& ctx, CodeGen& cg, Statements* out);

class FnGenerator final : public Generator {
 public:
  FnGenerator(std::string name, std::string_view description, double weight,
              uint32_t requirements, GenFn fn)
      : Generator(std::move(name), description, weight, requirements),
        fn_(fn) {}

  bool Generate(GenContext& ctx, Statements* out) override {
    CodeGen cg(ctx);
    return fn_(ctx, cg, out);
  }

 private:
  GenFn fn_;
};

// Generators share one signature; not every generator needs every argument.
#define GEN(name)                                  \
  bool Gen##name([[maybe_unused]] GenContext& ctx, \
                 [[maybe_unused]] CodeGen& cg, Statements* out)

// Shorthands.
Tree* T(GenContext& ctx) { return ctx.tree(); }

Statement* Push(Statements* out, Statement* s) {
  out->push_back(s);
  return s;
}

// A loop counter bound that is small most of the time.
double LoopCount(Random& r) {
  // Mostly small: a handful of iterations is enough to warm up feedback,
  // and %OptimizeOsr()/%OptimizeFunctionOnNextCall() replace long warm-up
  // loops (which only cost time and timeouts).
  static const double kCounts[] = {1,  2,  3,  3,  4,  5,   5,   8,
                                   10, 10, 16, 20, 32, 100, 1000};
  return kCounts[r.Index(sizeof(kCounts) / sizeof(kCounts[0]))];
}

REDLINE_TUNABLE(kInnerBodyMaxStatements, "generators.InnerBody.max_statements",
                3, 1, 64,
                "most statements in a nested if/try body unless the generator "
                "asks for another bound");

// A nested body for a loop/if/try at this position.
Statements InnerBody(GenContext& ctx, CodeGen& cg, bool in_loop, bool in_switch,
                     int max_statements = kInnerBodyMaxStatements.AsInt()) {
  GenContext inner = ctx.Inner(in_loop, in_switch);
  return cg.Body(inner, 1 + ctx.rng().SmallCount(max_statements - 1));
}

// --- value declarations
// ---------------------------------------------------------

GEN(WasmModule) {
  WasmModule m = RandomWasmModule(ctx.rng());

  // Create Uint8Array with the bytes
  std::vector<Expression*> byte_exprs;
  byte_exprs.reserve(m.bytes.size());
  for (uint8_t b : m.bytes) {
    byte_exprs.push_back(mk::Num(T(ctx), b));
  }
  Expression* arr = mk::Array(T(ctx), byte_exprs);
  Expression* typed_arr = mk::New(T(ctx), mk::Id(T(ctx), "Uint8Array"), {arr});

  std::string_view bytes_name = ctx.builder.FreshName("bytes");
  Statement* bytes_decl = mk::Const(T(ctx), bytes_name, typed_arr);
  Push(out, bytes_decl);
  ctx.Declare(bytes_name, JsType::kObject, true);

  // Create import object {m: {f0: ..., f1: ...}}
  std::vector<Node*> m_props;
  for (size_t i = 0; i < m.imports.size(); i++) {
    Expression* func_expr = cg.Callable();
    if (!func_expr) {
      func_expr = mk::ArrowExpr(T(ctx), {}, mk::Num(T(ctx), 0));
    }
    std::string fname = "f" + std::to_string(i);
    m_props.push_back(mk::Prop(T(ctx), fname, func_expr));
  }
  Expression* m_obj = mk::Object(T(ctx), m_props);
  Expression* import_obj = mk::Object(T(ctx), {mk::Prop(T(ctx), "m", m_obj)});

  // new WebAssembly.Module(bytes)
  Expression* module_expr = mk::New(
      T(ctx), mk::Member(T(ctx), mk::Id(T(ctx), "WebAssembly"), "Module"),
      {mk::Id(T(ctx), bytes_name)});

  std::string_view module_name = ctx.builder.FreshName("module");
  Statement* module_decl = mk::Const(T(ctx), module_name, module_expr);
  Push(out, module_decl);
  ctx.Declare(module_name, JsType::kObject, true);

  // new WebAssembly.Instance(module, import_obj)
  Expression* instance_expr = mk::New(
      T(ctx), mk::Member(T(ctx), mk::Id(T(ctx), "WebAssembly"), "Instance"),
      {mk::Id(T(ctx), module_name), import_obj});
  std::string_view instance_name = ctx.builder.FreshName("instance");
  Statement* instance_decl = mk::Const(T(ctx), instance_name, instance_expr);
  Push(out, instance_decl);
  ctx.Declare(instance_name, JsType::kObject, true);

  // exports = instance.exports
  Expression* exports_expr =
      mk::Member(T(ctx), mk::Id(T(ctx), instance_name), "exports");
  std::string_view exports_name = ctx.builder.FreshName("exports");
  Statement* exports_decl = mk::Const(T(ctx), exports_name, exports_expr);
  Push(out, exports_decl);
  ctx.Declare(exports_name, JsType::kObject, true);

  // exported functions
  for (size_t i = 0; i < m.functions.size(); i++) {
    std::string ename = "e" + std::to_string(i);
    Expression* func_load =
        mk::Member(T(ctx), mk::Id(T(ctx), exports_name), ename);
    std::string_view func_name = ctx.builder.FreshName("f");
    Statement* func_decl = mk::Const(T(ctx), func_name, func_load);
    Push(out, func_decl);
    ctx.Declare(func_name, JsType::kFunction, true);

    // Call it 1-3 times
    int calls = 1 + static_cast<int>(ctx.rng().Index(3));
    for (int c = 0; c < calls; ++c) {
      std::vector<Expression*> args;
      for (auto t : m.functions[i].params) {
        if (t == WasmType::kI64) {
          args.push_back(
              mk::BigInt(T(ctx), std::to_string(ctx.rng().Range(0, 100))));
        } else {
          args.push_back(mk::Num(T(ctx), ctx.rng().Double() * 100));
        }
      }
      Expression* call_expr = mk::Call(T(ctx), mk::Id(T(ctx), func_name), args);
      Push(out, cg.Stmt(call_expr));
    }
  }

  if (m.memory) {
    Expression* mem_load =
        mk::Member(T(ctx), mk::Id(T(ctx), exports_name), "mem");
    std::string_view mem_name = ctx.builder.FreshName("mem");
    Statement* mem_decl = mk::Const(T(ctx), mem_name, mem_load);
    Push(out, mem_decl);
    ctx.Declare(mem_name, JsType::kObject, true);

    if (ctx.rng().Coin()) {
      Expression* buf_load =
          mk::Member(T(ctx), mk::Id(T(ctx), mem_name), "buffer");
      Expression* view_expr =
          mk::New(T(ctx), mk::Id(T(ctx), "Uint8Array"), {buf_load});
      std::string_view view_name = ctx.builder.FreshName("view");
      Statement* view_decl = mk::Const(T(ctx), view_name, view_expr);
      Push(out, view_decl);
      ctx.Declare(view_name, JsType::kTypedArray, true);
    } else {
      Expression* grow_call = mk::CallMethod(T(ctx), mk::Id(T(ctx), mem_name),
                                             "grow", {mk::Num(T(ctx), 1)});
      Push(out, cg.Stmt(grow_call));
    }
  }

  if (m.table) {
    Expression* tab_load =
        mk::Member(T(ctx), mk::Id(T(ctx), exports_name), "tab");
    std::string_view tab_name = ctx.builder.FreshName("tab");
    Statement* tab_decl = mk::Const(T(ctx), tab_name, tab_load);
    Push(out, tab_decl);
    ctx.Declare(tab_name, JsType::kObject, true);

    if (ctx.rng().Coin()) {
      Expression* get_call = mk::CallMethod(T(ctx), mk::Id(T(ctx), tab_name),
                                            "get", {mk::Num(T(ctx), 0)});
      Push(out, cg.Stmt(get_call));
    } else {
      Expression* len_load =
          mk::Member(T(ctx), mk::Id(T(ctx), tab_name), "length");
      Push(out, cg.Stmt(len_load));
    }
  }

  int g_idx = 0;
  for (auto t : m.globals) {
    std::string gname = "g" + std::to_string(g_idx++);
    Expression* glob_load =
        mk::Member(T(ctx), mk::Id(T(ctx), exports_name), gname);
    std::string_view glob_name = ctx.builder.FreshName("g");
    Statement* glob_decl = mk::Const(T(ctx), glob_name, glob_load);
    Push(out, glob_decl);
    ctx.Declare(glob_name, JsType::kObject, true);

    if (ctx.rng().Coin()) {
      Expression* val_load =
          mk::Member(T(ctx), mk::Id(T(ctx), glob_name), "value");
      Push(out, cg.Stmt(val_load));
    } else {
      Expression* val_store = mk::Assign(
          T(ctx), mk::Member(T(ctx), mk::Id(T(ctx), glob_name), "value"),
          (t == WasmType::kI64)
              ? static_cast<Expression*>(mk::BigInt(T(ctx), "42"))
              : static_cast<Expression*>(mk::Num(T(ctx), 42)));
      Push(out, cg.Stmt(val_store));
    }
  }

  return true;
}

GEN(IntegerDecl) {
  Push(out, cg.Declare(cg.Integer(), JsType::kNumber));
  return true;
}

REDLINE_TUNABLE(kFloatDeclInterestingChance,
                "generators.FloatDecl.interesting_chance", 0.5, 0, 1,
                "probability that the number is one of the environment's "
                "interesting values rather than a fresh literal");

GEN(FloatDecl) {
  Random& r = ctx.rng();
  Expression* v = r.Chance(kFloatDeclInterestingChance())
                      ? mk::Num(T(ctx), r.Pick(ctx.env.numbers()))
                      : cg.Number();
  Push(out, cg.Declare(v, JsType::kNumber));
  return true;
}

GEN(StringDecl) {
  Push(out, cg.Declare(cg.String(), JsType::kString, "s"));
  return true;
}

GEN(BooleanDecl) {
  Push(out, cg.Declare(cg.Boolean(), JsType::kBoolean, "b"));
  return true;
}

GEN(BigIntDecl) {
  Push(out, cg.Declare(cg.BigInt(), JsType::kBigInt, "n"));
  return true;
}

GEN(RegExpDecl) {
  Push(out, cg.Declare(cg.RegExp(), JsType::kRegExp, "re"));
  return true;
}

GEN(NullishDecl) {
  Push(out, cg.Declare(cg.Nullish(), JsType::kNullish));
  return true;
}

GEN(SymbolDecl) {
  Random& r = ctx.rng();
  Expression* v = nullptr;
  if (r.Coin()) {
    v = mk::Call(T(ctx), mk::Id(T(ctx), "Symbol"), {cg.String()});
  } else {
    v = mk::Member(T(ctx), mk::Id(T(ctx), "Symbol"),
                   r.Pick({"iterator", "toPrimitive", "hasInstance", "species",
                           "toStringTag", "asyncIterator"}));
  }
  Push(out, cg.Declare(v, JsType::kSymbol, "sym"));
  return true;
}

GEN(ArrayDecl) {
  Push(out, cg.Declare(cg.ArrayLiteral(0), JsType::kArray, "arr"));
  return true;
}

GEN(ObjectDecl) {
  Push(out, cg.Declare(cg.ObjectLiteral(0), JsType::kObject, "obj"));
  return true;
}

GEN(ConstructDecl) {
  Random& r = ctx.rng();
  std::vector<const BuiltinGlobal*> ctors;
  for (const BuiltinGlobal& g : ctx.env.globals()) {
    if (g.constructible) {
      ctors.push_back(&g);
    }
  }
  if (ctors.empty()) {
    return false;
  }
  const BuiltinGlobal& g = *r.Pick(ctors);
  Expression* v = cg.Construct(g.instance_type);
  if (v == nullptr) {
    return false;
  }
  Push(out, cg.Declare(v, g.instance_type, "o"));
  return true;
}

GEN(TypedArrayDecl) {
  Expression* v = cg.Construct(JsType::kTypedArray);
  if (v == nullptr) {
    return false;
  }
  Push(out, cg.Declare(v, JsType::kTypedArray, "ta"));
  return true;
}

GEN(ComputedDecl) {
  Expression* v = cg.Expression(0);
  Push(out, cg.Declare(v, Builder::GuessType(v)));
  return true;
}

REDLINE_TUNABLE(kTemplateDeclMinSubstitutions,
                "generators.TemplateDecl.min_substitutions", 1, 0, 64,
                "fewest ${...} substitutions in the template literal");
REDLINE_TUNABLE(kTemplateDeclMaxSubstitutions,
                "generators.TemplateDecl.max_substitutions", 3, 1, 64,
                "most ${...} substitutions in the template literal");

REDLINE_TUNABLE(kTemplateDeclTaggedChance,
                "generators.TemplateDecl.tagged_chance", 0.2, 0, 1,
                "probability that the template is tagged (tag`...`) with a "
                "function variable or String.raw");

GEN(TemplateDecl) {
  Random& r = ctx.rng();
  std::vector<std::string_view> strings;
  std::vector<Expression*> expressions;
  int n = static_cast<int>(r.Range(kTemplateDeclMinSubstitutions.AsInt(),
                                   kTemplateDeclMaxSubstitutions.AsInt()));
  static const char* const kParts[] = {"",    "a",  "b",  " ",
                                       "foo", "x=", ", ", "!"};
  for (int i = 0; i < n; i++) {
    strings.emplace_back(kParts[r.Index(sizeof(kParts) / sizeof(kParts[0]))]);
    expressions.push_back(cg.Value(JsType::kUnknown, 1));
  }
  strings.emplace_back(kParts[r.Index(sizeof(kParts) / sizeof(kParts[0]))]);
  TemplateLiteral* literal =
      mk::Template(T(ctx), std::move(strings), std::move(expressions));
  if (r.Chance(kTemplateDeclTaggedChance())) {
    // The tag gets the array of strings and the substituted values.
    Expression* tag = cg.Variable(JsType::kFunction);
    if (tag == nullptr) {
      tag = mk::Member(T(ctx), mk::Id(T(ctx), "String"), "raw");
    }
    TaggedTemplateExpression* tagged = T(ctx)->New<TaggedTemplateExpression>();
    tagged->tag = tag;
    tagged->quasi = literal;
    Push(out, cg.Declare(tagged, JsType::kUnknown, "s"));
    return true;
  }
  Push(out, cg.Declare(literal, JsType::kString, "s"));
  return true;
}

REDLINE_TUNABLE(kDestructuringMinProperties,
                "generators.Destructuring.min_properties", 1, 0, 64,
                "fewest properties in an object destructuring pattern");
REDLINE_TUNABLE(kDestructuringMaxProperties,
                "generators.Destructuring.max_properties", 3, 1, 64,
                "most properties in an object destructuring pattern");
REDLINE_TUNABLE(kDestructuringMinElements,
                "generators.Destructuring.min_elements", 1, 0, 64,
                "fewest elements in an array destructuring pattern");
REDLINE_TUNABLE(kDestructuringMaxElements,
                "generators.Destructuring.max_elements", 4, 1, 64,
                "most elements in an array destructuring pattern");
REDLINE_TUNABLE(kDestructuringDefaultChance,
                "generators.Destructuring.default_chance", 0.3, 0, 1,
                "probability that a destructured binding gets a default value");
REDLINE_TUNABLE(kDestructuringRestChance,
                "generators.Destructuring.rest_chance", 0.3, 0, 1,
                "probability that the pattern ends with a ...rest element");
REDLINE_TUNABLE(kDestructuringHoleChance,
                "generators.Destructuring.hole_chance", 0.15, 0, 1,
                "probability that an array pattern element is a hole");

GEN(Destructuring) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (r.Coin()) {
    // let {a, b: v1, ...rest} = obj;
    Expression* source = cg.ObjectLike();
    ObjectPattern* pattern = t->New<ObjectPattern>();
    int n = static_cast<int>(r.Range(kDestructuringMinProperties.AsInt(),
                                     kDestructuringMaxProperties.AsInt()));
    for (int i = 0; i < n; i++) {
      std::string_view key = cg.PropertyName();
      if (!IsIdentifierName(key)) {
        key = "a";
      }
      std::string_view name = ctx.builder.FreshName("d");
      Property* p = mk::Prop(t, key, mk::Id(t, name));
      if (r.Chance(kDestructuringDefaultChance())) {
        AssignmentPattern* ap = t->New<AssignmentPattern>();
        ap->left = mk::Id(t, name);
        ap->right = cg.Value(JsType::kUnknown, 1);
        p->value = ap;
      }
      pattern->properties.push_back(p);
      ctx.Declare(name, JsType::kUnknown);
    }
    if (r.Chance(kDestructuringRestChance())) {
      RestElement* rest = t->New<RestElement>();
      std::string_view name = ctx.builder.FreshName("rest");
      rest->argument = mk::Id(t, name);
      pattern->properties.push_back(rest);
      ctx.Declare(name, JsType::kObject);
    }
    VariableDeclaration* decl = mk::Let(t, "x", source);
    decl->declarations[0]->target = pattern;
    Push(out, decl);
  } else {
    // let [a, , b = 1, ...rest] = arr;
    Expression* source = nullptr;
    if (Identifier* id = cg.Variable(JsType::kArray)) {
      source = id;
    } else if (Identifier* id2 = cg.Variable(JsType::kString)) {
      source = id2;
    } else {
      source = cg.ArrayLiteral(1);
    }
    ArrayPattern* pattern = t->New<ArrayPattern>();
    int n = static_cast<int>(r.Range(kDestructuringMinElements.AsInt(),
                                     kDestructuringMaxElements.AsInt()));
    for (int i = 0; i < n; i++) {
      if (r.Chance(kDestructuringHoleChance())) {
        pattern->elements.push_back(nullptr);
        continue;
      }
      std::string_view name = ctx.builder.FreshName("d");
      if (r.Chance(kDestructuringDefaultChance())) {
        AssignmentPattern* ap = t->New<AssignmentPattern>();
        ap->left = mk::Id(t, name);
        ap->right = cg.Value(JsType::kUnknown, 1);
        pattern->elements.push_back(ap);
      } else {
        pattern->elements.push_back(mk::Id(t, name));
      }
      ctx.Declare(name, JsType::kUnknown);
    }
    if (r.Chance(kDestructuringRestChance())) {
      RestElement* rest = t->New<RestElement>();
      std::string_view name = ctx.builder.FreshName("rest");
      rest->argument = mk::Id(t, name);
      pattern->elements.push_back(rest);
      ctx.Declare(name, JsType::kArray);
    }
    VariableDeclaration* decl = mk::Let(t, "x", source);
    decl->declarations[0]->target = pattern;
    Push(out, decl);
  }
  return true;
}

// --- functions and classes
// ------------------------------------------------------

REDLINE_TUNABLE(kFunctionDeclMaxParams, "generators.FunctionDecl.max_params", 3,
                0, 16, "most parameters of the function");
REDLINE_TUNABLE(kFunctionDeclAsyncChance,
                "generators.FunctionDecl.async_chance", 0.1, 0, 1,
                "probability that the function is async");
REDLINE_TUNABLE(kFunctionDeclGeneratorChance,
                "generators.FunctionDecl.generator_chance", 0.08, 0, 1,
                "probability that the function is a generator");
REDLINE_TUNABLE(
    kFunctionDeclMaxStatements, "generators.FunctionDecl.max_statements", 5, 1,
    64, "most statements in the function body before the optional return");
REDLINE_TUNABLE(kFunctionDeclReturnChance,
                "generators.FunctionDecl.return_chance", 0.8, 0, 1,
                "probability that the body ends with a return of a value");
REDLINE_TUNABLE(
    kFunctionDeclCallChance, "generators.FunctionDecl.call_chance", 0.7, 0, 1,
    "probability that the function is called right after its declaration");
REDLINE_TUNABLE(kFunctionDeclMaxCalls, "generators.FunctionDecl.max_calls", 3,
                1, 64, "most calls emitted right after the declaration");

GEN(FunctionDecl) {
  Random& r = ctx.rng();
  std::string_view name = ctx.builder.FreshName("f");
  std::vector<std::string_view> params;
  int n = static_cast<int>(r.Range(0, kFunctionDeclMaxParams.AsInt()));
  bool is_async = r.Chance(kFunctionDeclAsyncChance());
  bool is_generator = r.Chance(kFunctionDeclGeneratorChance());
  GenContext nested = ctx.Nested(true, false, is_async, is_generator);
  // The function may call itself.
  nested.Declare(name, JsType::kFunction);
  for (int i = 0; i < n; i++) {
    std::string_view p = ctx.builder.FreshName("a");
    params.push_back(p);
    nested.Declare(p, JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body = inner.Body(
      nested, 1 + r.SmallCount(kFunctionDeclMaxStatements.AsInt() - 1));
  if (r.Chance(kFunctionDeclReturnChance())) {
    body.push_back(mk::Return(T(ctx), inner.Value(JsType::kUnknown, 1)));
  }
  Push(out, mk::FunctionDecl(T(ctx), name, params, std::move(body), is_async,
                             is_generator));
  ctx.Declare(name, JsType::kFunction);
  // Use it right away most of the time.
  if (r.Chance(kFunctionDeclCallChance())) {
    int calls = 1 + r.SmallCount(kFunctionDeclMaxCalls.AsInt() - 1);
    for (int i = 0; i < calls; i++) {
      Push(out, cg.Stmt(cg.CallWithArguments(mk::Id(T(ctx), name), n + 1)));
    }
  }
  return true;
}

GEN(ArrowDecl) {
  Expression* fn = cg.FunctionValue(ctx.depth);
  Push(out, cg.Declare(fn, JsType::kFunction, "f"));
  return true;
}

REDLINE_TUNABLE(
    kClassDeclSuperClassChance, "generators.ClassDecl.super_class_chance", 0.4,
    0, 1, "probability that the class extends a visible class or a builtin");
REDLINE_TUNABLE(kClassDeclConstructorMaxParams,
                "generators.ClassDecl.constructor_max_params", 2, 0, 16,
                "most parameters of the constructor");
REDLINE_TUNABLE(kClassDeclConstructorMaxFields,
                "generators.ClassDecl.constructor_max_fields", 3, 1, 64,
                "most this.x = ... initializations in the constructor");
REDLINE_TUNABLE(
    kClassDeclConstructorMaxStatements,
    "generators.ClassDecl.constructor_max_statements", 2, 0, 64,
    "most extra statements in the constructor after the field initializations");
REDLINE_TUNABLE(kClassDeclManyMembersChance,
                "generators.ClassDecl.many_members_chance", 0.05, 0, 1,
                "probability that a class has many members");
REDLINE_TUNABLE(kClassDeclManyMembersCount,
                "generators.ClassDecl.many_members_count", 50, 10, 200,
                "most members when the class has many");
REDLINE_TUNABLE(kClassDeclMaxMembers, "generators.ClassDecl.max_members", 4, 1,
                64,
                "most methods, accessors and fields besides the constructor");
REDLINE_TUNABLE(kClassDeclStaticChance, "generators.ClassDecl.static_chance",
                0.2, 0, 1, "probability that a member is static");
REDLINE_TUNABLE(kClassDeclMethodAsyncChance,
                "generators.ClassDecl.method_async_chance", 0.1, 0, 1,
                "probability that a method is async");
REDLINE_TUNABLE(kClassDeclMethodGeneratorChance,
                "generators.ClassDecl.method_generator_chance", 0.1, 0, 1,
                "probability that a method is a generator");
REDLINE_TUNABLE(kClassDeclMethodMaxParams,
                "generators.ClassDecl.method_max_params", 2, 0, 16,
                "most parameters of a method");
REDLINE_TUNABLE(kClassDeclMethodMaxStatements,
                "generators.ClassDecl.method_max_statements", 3, 1, 64,
                "most statements in a method body before its return");
REDLINE_TUNABLE(kClassDeclMethodThisStoreChance,
                "generators.ClassDecl.method_this_store_chance", 0.3, 0, 1,
                "probability that a method stores to this.x");
REDLINE_TUNABLE(
    kClassDeclSuperCallChance, "generators.ClassDecl.super_call_chance", 0.3, 0,
    1, "probability that a method of a derived class calls a super method");
REDLINE_TUNABLE(
    kClassDeclInstantiateChance, "generators.ClassDecl.instantiate_chance", 0.8,
    0, 1,
    "probability that the class is instantiated right after its declaration");
REDLINE_TUNABLE(kClassDeclInstantiateMaxArgs,
                "generators.ClassDecl.instantiate_max_args", 2, 0, 16,
                "most arguments passed to new C(...)");

REDLINE_TUNABLE(kClassDeclExpressionChance,
                "generators.ClassDecl.expression_chance", 0.15, 0, 1,
                "probability that the class is a class expression bound to a "
                "const (const C = class {...})");
REDLINE_TUNABLE(kClassDeclStaticBlockChance,
                "generators.ClassDecl.static_block_chance", 0.15, 0, 1,
                "probability that the class gets a static { ... } block");
REDLINE_TUNABLE(kClassDeclStaticBlockMaxStatements,
                "generators.ClassDecl.static_block_max_statements", 3, 1, 64,
                "most statements in the static block");

GEN(ClassDecl) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view name = ctx.builder.FreshName("C");
  Expression* super_class = nullptr;
  if (r.Chance(kClassDeclSuperClassChance())) {
    if (Identifier* id = cg.Variable(JsType::kClass)) {
      super_class = id;
    } else if (r.Coin()) {
      super_class =
          mk::Id(t, r.Pick({"Array", "Error", "Map", "Set", "Object", "Promise",
                            "Uint8Array", "Function", "RegExp", "Date"}));
    }
  }
  std::vector<Node*> members;
  // Constructor.
  {
    GenContext nested = ctx.Nested(true, false, false, false);
    nested.syntax.strict = true;
    nested.syntax.in_method = true;
    nested.syntax.in_constructor = true;
    nested.syntax.has_super_class = super_class != nullptr;
    std::vector<std::string_view> params;
    int n =
        static_cast<int>(r.Range(0, kClassDeclConstructorMaxParams.AsInt()));
    for (int i = 0; i < n; i++) {
      std::string_view p = ctx.builder.FreshName("a");
      params.push_back(p);
      nested.Declare(p, JsType::kUnknown);
    }
    CodeGen inner(nested);
    Statements body;
    if (super_class != nullptr) {
      std::vector<Expression*> args;
      args.reserve(params.size());
      for (std::string_view p : params) {
        args.push_back(mk::Id(t, p));
      }
      body.push_back(inner.Stmt(mk::Call(t, t->New<Super>(), std::move(args))));
    }
    int fields = 1 + r.SmallCount(kClassDeclConstructorMaxFields.AsInt() - 1);
    for (int i = 0; i < fields; i++) {
      body.push_back(inner.Stmt(
          mk::Assign(t, inner.Property(mk::This(t), inner.PropertyName()),
                     inner.Value(JsType::kUnknown, 1))));
    }
    Statements more = inner.Body(
        nested, r.SmallCount(kClassDeclConstructorMaxStatements.AsInt()));
    body.insert(body.end(), more.begin(), more.end());
    members.push_back(mk::ClassMethod(
        t, "constructor", mk::Function(t, "", params, std::move(body)),
        MethodKind::kConstructor));
  }
  // Methods, accessors, fields.
  int n = 1 + r.SmallCount(kClassDeclMaxMembers.AsInt() - 1);
  if (r.Chance(kClassDeclManyMembersChance())) {
    n = static_cast<int>(r.Range(10, kClassDeclManyMembersCount.AsInt()));
  }
  for (int i = 0; i < n; i++) {
    std::string_view mname = ctx.builder.FreshName("m");
    bool is_static = r.Chance(kClassDeclStaticChance());
    switch (r.Index(5)) {
      case 0: {
        // Field.
        GenContext nested = ctx.Nested(true, true, false, false);
        nested.syntax.strict = true;
        nested.syntax.in_class_field = true;
        nested.syntax.has_arguments = false;
        nested.syntax.in_method = true;
        CodeGen inner(nested);
        members.push_back(mk::ClassField(
            t, mname, inner.Value(JsType::kUnknown, 1), is_static));
        break;
      }
      case 1: {
        // Getter (+ setter).
        GenContext nested = ctx.Nested(true, false, false, false);
        nested.syntax.strict = true;
        nested.syntax.in_method = true;
        nested.syntax.has_super_class = super_class != nullptr;
        CodeGen inner(nested);
        Statements body = {mk::Return(t, inner.Value(JsType::kUnknown, 1))};
        members.push_back(
            mk::ClassMethod(t, mname, mk::Function(t, "", {}, std::move(body)),
                            MethodKind::kGet, is_static));
        if (r.Coin()) {
          std::string_view p = ctx.builder.FreshName("v");
          GenContext setter = ctx.Nested(true, false, false, false);
          setter.syntax.strict = true;
          setter.syntax.in_method = true;
          setter.Declare(p, JsType::kUnknown);
          CodeGen sg(setter);
          Statements sbody = {sg.Stmt(
              mk::Assign(t, mk::Member(t, mk::This(t), "x"), mk::Id(t, p)))};
          members.push_back(mk::ClassMethod(
              t, mname, mk::Function(t, "", {p}, std::move(sbody)),
              MethodKind::kSet, is_static));
        }
        break;
      }
      default: {
        // Method.
        GenContext nested =
            ctx.Nested(true, false, r.Chance(kClassDeclMethodAsyncChance()),
                       r.Chance(kClassDeclMethodGeneratorChance()));
        nested.syntax.strict = true;
        nested.syntax.in_method = true;
        nested.syntax.has_super_class = super_class != nullptr;
        std::vector<std::string_view> params;
        int np =
            static_cast<int>(r.Range(0, kClassDeclMethodMaxParams.AsInt()));
        for (int p = 0; p < np; p++) {
          std::string_view pn = ctx.builder.FreshName("a");
          params.push_back(pn);
          nested.Declare(pn, JsType::kUnknown);
        }
        CodeGen inner(nested);
        Statements body = inner.Body(
            nested,
            1 + r.SmallCount(kClassDeclMethodMaxStatements.AsInt() - 1));
        if (r.Chance(kClassDeclMethodThisStoreChance())) {
          body.push_back(
              inner.Stmt(mk::Assign(t, mk::Member(t, mk::This(t), "x"),
                                    inner.Value(JsType::kUnknown, 1))));
        }
        if (super_class != nullptr && r.Chance(kClassDeclSuperCallChance())) {
          MemberExpression* sm = mk::Member(
              t, t->New<Super>(), r.Pick({"toString", "valueOf", "x", "m"}));
          body.push_back(inner.Stmt(mk::Call(t, sm, {})));
        }
        body.push_back(mk::Return(t, r.Coin()
                                         ? static_cast<Expression*>(mk::This(t))
                                         : inner.Value(JsType::kUnknown, 1)));
        members.push_back(mk::ClassMethod(
            t, mname,
            mk::Function(t, "", params, std::move(body), nested.syntax.in_async,
                         nested.syntax.in_generator),
            MethodKind::kMethod, is_static));
        break;
      }
    }
  }
  if (r.Chance(kClassDeclStaticBlockChance())) {
    // static { ... }: runs once when the class is defined; `this` is the
    // class. The same context as the builder computes for a static block
    // of an imported program (Builder::ContextAtSlot).
    GenContext nested = ctx.Nested(false, false, false, false);
    nested.syntax.in_static_block = true;
    nested.syntax.in_method = true;
    nested.syntax.strict = true;
    nested.syntax.in_function = false;
    nested.syntax.in_async = false;
    nested.syntax.in_generator = false;
    nested.syntax.in_loop = false;
    nested.syntax.in_switch = false;
    nested.syntax.in_constructor = false;
    nested.syntax.has_arguments = false;
    nested.syntax.has_super_class = super_class != nullptr;
    nested.syntax.labels.clear();
    CodeGen inner(nested);
    StaticBlock* block = t->New<StaticBlock>();
    block->body = inner.Body(
        nested,
        1 + r.SmallCount(kClassDeclStaticBlockMaxStatements.AsInt() - 1));
    members.push_back(block);
  }
  if (r.Chance(kClassDeclExpressionChance())) {
    Push(out,
         mk::Const(t, name, mk::Class(t, "", super_class, std::move(members))));
  } else {
    Push(out, mk::ClassDecl(t, name, super_class, std::move(members)));
  }
  ctx.Declare(name, JsType::kClass, true);
  // Instantiate it.
  if (r.Chance(kClassDeclInstantiateChance())) {
    Push(out, cg.Declare(mk::New(t, mk::Id(t, name),
                                 cg.Arguments(
                                     0, kClassDeclInstantiateMaxArgs.AsInt())),
                         JsType::kObject, "o"));
  }
  return true;
}

REDLINE_TUNABLE(kIifeAsyncChance, "generators.Iife.async_chance", 0.2, 0, 1,
                "probability that the IIFE is async");
REDLINE_TUNABLE(kIifeMaxStatements, "generators.Iife.max_statements", 4, 1, 64,
                "most statements in the IIFE body");

GEN(Iife) {
  Random& r = ctx.rng();
  bool is_async = r.Chance(kIifeAsyncChance());
  bool is_arrow = r.Coin();
  GenContext nested = ctx.Nested(true, is_arrow, is_async, false);
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, 1 + r.SmallCount(kIifeMaxStatements.AsInt() - 1));
  Expression* fn = is_arrow
                       ? static_cast<Expression*>(
                             mk::Arrow(T(ctx), {}, std::move(body), is_async))
                       : static_cast<Expression*>(mk::Function(
                             T(ctx), "", {}, std::move(body), is_async, false));
  Push(out, cg.Stmt(mk::Call(T(ctx), fn, {})));
  return true;
}

// --- operations
// -----------------------------------------------------------------

GEN(BinaryOp) {
  Expression* v = cg.BinaryOperation(0);
  Push(out, cg.Declare(v, Builder::GuessType(v)));
  return true;
}

GEN(Compare) {
  Push(out, cg.Declare(cg.Comparison(0), JsType::kBoolean, "b"));
  return true;
}

GEN(UnaryOp) {
  Expression* v = cg.UnaryOperation(0);
  if (v->Is<UpdateExpression>() ||
      (v->Is<UnaryExpression>() &&
       v->Cast<UnaryExpression>()->op == UnaryOp::kDelete)) {
    Push(out, cg.Stmt(v));
    return true;
  }
  Push(out, cg.Declare(v, Builder::GuessType(v)));
  return true;
}

GEN(Update) {
  Random& r = ctx.rng();
  Expression* target = cg.MutableVariable(JsType::kNumber);
  if (target == nullptr) {
    target = cg.Target();
  }
  if (target->Is<Identifier>() &&
      target->Cast<Identifier>()->name == "arguments") {
    return false;
  }
  Push(out, cg.Stmt(mk::Update(
                T(ctx), r.Coin() ? UpdateOp::kIncrement : UpdateOp::kDecrement,
                r.Coin(), target)));
  return true;
}

REDLINE_TUNABLE(
    kAssignmentPlainChance, "generators.Assignment.plain_chance", 0.6, 0, 1,
    "probability of a plain = rather than a compound assignment operator");
REDLINE_TUNABLE(kAssignmentNumberHintChance,
                "generators.Assignment.number_hint_chance", 0.7, 0, 1,
                "probability that the right-hand side of a compound assignment "
                "is generated as a number");

GEN(Assignment) {
  Random& r = ctx.rng();
  Expression* target = cg.Target();
  AssignOp op = r.Chance(kAssignmentPlainChance())
                    ? AssignOp::kAssign
                    : CodeGen::RandomCompoundAssignOp(r);
  JsType hint = JsType::kUnknown;
  if (target->Is<Identifier>()) {
    for (const Variable& v : ctx.visible) {
      if (v.name == target->Cast<Identifier>()->name) {
        hint = v.type;
      }
    }
  }
  if (op != AssignOp::kAssign && r.Chance(kAssignmentNumberHintChance())) {
    hint = JsType::kNumber;
  }
  Push(out, cg.Stmt(mk::Assign(T(ctx), target, cg.Value(hint, 0), op)));
  return true;
}

// `object?.name` or `object?.[key]` from a property load.
Expression* OptionalChain(Tree* t, Expression* load) {
  if (!load->Is<MemberExpression>()) {
    return load;
  }
  load->Cast<MemberExpression>()->optional = true;
  return mk::Chain(t, load);
}

REDLINE_TUNABLE(kPropertyLoadOptionalChance,
                "generators.PropertyLoad.optional_chance", 0.1, 0, 1,
                "probability that the load is an optional chain (o?.p)");

GEN(PropertyLoad) {
  Expression* v = cg.Property(cg.ObjectLike());
  if (ctx.rng().Chance(kPropertyLoadOptionalChance())) {
    v = OptionalChain(T(ctx), v);
  }
  Push(out, cg.Declare(v, JsType::kUnknown));
  return true;
}

GEN(PropertyStore) {
  Expression* object = cg.ObjectLike();
  if (object->Is<ObjectExpression>() || object->Is<ArrayExpression>()) {
    // Bind literals first so that the store is observable.
    Statement* decl = cg.Declare(object, JsType::kObject, "obj");
    Push(out, decl);
    object = mk::Id(T(ctx), decl->Cast<VariableDeclaration>()
                                ->declarations[0]
                                ->target->Cast<Identifier>()
                                ->name);
  }
  Push(out, cg.Stmt(mk::Assign(T(ctx), cg.Property(object),
                               cg.Value(JsType::kUnknown, 0))));
  return true;
}

REDLINE_TUNABLE(kElementLoadVariableKeyChance,
                "generators.ElementLoad.variable_key_chance", 0.6, 0, 1,
                "probability that the index is a visible number variable "
                "rather than a generated key");
REDLINE_TUNABLE(kElementLoadOptionalChance,
                "generators.ElementLoad.optional_chance", 0.1, 0, 1,
                "probability that the load is an optional chain (a?.[i])");

GEN(ElementLoad) {
  Random& r = ctx.rng();
  Expression* object = nullptr;
  if (Identifier* id =
          cg.Variable(r.Coin() ? JsType::kArray : JsType::kTypedArray)) {
    object = id;
  } else {
    object = cg.ObjectLike();
  }
  Expression* key = nullptr;
  if (r.Chance(kElementLoadVariableKeyChance())) {
    if (Identifier* id = cg.Variable(JsType::kNumber)) {
      key = id;
    }
  }
  if (key == nullptr) {
    key = cg.Key();
  }
  Expression* load = mk::Index(T(ctx), object, key);
  if (r.Chance(kElementLoadOptionalChance())) {
    load = OptionalChain(T(ctx), load);
  }
  Push(out, cg.Declare(load, JsType::kUnknown));
  return true;
}

REDLINE_TUNABLE(kElementStoreVariableKeyChance,
                "generators.ElementStore.variable_key_chance", 0.6, 0, 1,
                "probability that the index is a visible number variable "
                "rather than a generated key");

GEN(ElementStore) {
  Random& r = ctx.rng();
  Expression* object = nullptr;
  if (Identifier* id =
          cg.Variable(r.Coin() ? JsType::kArray : JsType::kTypedArray)) {
    object = id;
  } else if (Identifier* id2 = cg.Variable(JsType::kObject)) {
    object = id2;
  } else {
    return false;
  }
  Expression* key = nullptr;
  if (r.Chance(kElementStoreVariableKeyChance())) {
    if (Identifier* id = cg.Variable(JsType::kNumber)) {
      key = id;
    }
  }
  if (key == nullptr) {
    key = cg.Key();
  }
  Push(out, cg.Stmt(mk::Assign(T(ctx), mk::Index(T(ctx), object, key),
                               cg.Value(JsType::kUnknown, 0))));
  return true;
}

GEN(PropertyDelete) {
  Identifier* object = cg.Variable(JsType::kObject);
  if (object == nullptr) {
    object = cg.Variable(JsType::kArray);
  }
  if (object == nullptr) {
    return false;
  }
  Push(out, cg.Stmt(mk::Unary(T(ctx), UnaryOp::kDelete, cg.Property(object))));
  return true;
}

REDLINE_TUNABLE(kMethodCallDeclareChance,
                "generators.MethodCall.declare_chance", 0.6, 0, 1,
                "probability that the call result is bound to a variable "
                "rather than discarded");

GEN(MethodCall) {
  Random& r = ctx.rng();
  Expression* object = cg.ObjectLike();
  Expression* call = cg.MethodCall(object, Builder::GuessType(object));
  if (r.Chance(kMethodCallDeclareChance())) {
    Push(out, cg.Declare(call, JsType::kUnknown));
  } else {
    Push(out, cg.Stmt(call));
  }
  return true;
}

REDLINE_TUNABLE(kFunctionCallUntypedCalleeChance,
                "generators.FunctionCall.untyped_callee_chance", 0.3, 0, 1,
                "probability of calling an untyped variable when no function "
                "variable is visible");
REDLINE_TUNABLE(kFunctionCallCallApplyChance,
                "generators.FunctionCall.call_apply_chance", 0.1, 0, 1,
                "probability of f.call(thisArg, ...)/f.apply(thisArg, [...]) "
                "rather than a direct call");
REDLINE_TUNABLE(kFunctionCallDeclareChance,
                "generators.FunctionCall.declare_chance", 0.5, 0, 1,
                "probability that the call result is bound to a variable "
                "rather than discarded");

GEN(FunctionCall) {
  Random& r = ctx.rng();
  Identifier* callee = cg.Variable(JsType::kFunction);
  if (callee == nullptr && r.Chance(kFunctionCallUntypedCalleeChance())) {
    callee = cg.Variable(JsType::kUnknown);
  }
  if (callee == nullptr) {
    return false;
  }
  Expression* call = cg.CallWithArguments(callee);
  if (r.Chance(kFunctionCallCallApplyChance())) {
    // f.call(thisArg, ...) / f.apply(thisArg, [...])
    if (r.Coin()) {
      call = mk::CallMethod(T(ctx), callee, "call",
                            {cg.ObjectLike(), cg.Value(JsType::kUnknown, 1)});
    } else {
      call = mk::CallMethod(T(ctx), callee, "apply",
                            {cg.ObjectLike(), cg.ArrayLiteral(1)});
    }
  }
  if (r.Chance(kFunctionCallDeclareChance())) {
    Push(out, cg.Declare(call, JsType::kUnknown));
  } else {
    Push(out, cg.Stmt(call));
  }
  return true;
}

REDLINE_TUNABLE(kBuiltinCallDeclareChance,
                "generators.BuiltinCall.declare_chance", 0.6, 0, 1,
                "probability that the call result is bound to a variable "
                "rather than discarded");

GEN(BuiltinCall) {
  Random& r = ctx.rng();
  Expression* call = cg.BuiltinCall();
  if (r.Chance(kBuiltinCallDeclareChance())) {
    Push(out, cg.Declare(call, Builder::GuessType(call)));
  } else {
    Push(out, cg.Stmt(call));
  }
  return true;
}

REDLINE_TUNABLE(kConstructMaxArgs, "generators.Construct.max_args", 2, 0, 16,
                "most arguments passed to new on a visible class or function");

GEN(Construct) {
  Random& r = ctx.rng();
  Expression* v = nullptr;
  if (r.Coin()) {
    if (Identifier* id = cg.Variable(JsType::kClass)) {
      v = mk::New(T(ctx), id, cg.Arguments(0, kConstructMaxArgs.AsInt()));
    }
  }
  if (v == nullptr && r.Coin()) {
    if (Identifier* id = cg.Variable(JsType::kFunction)) {
      v = mk::New(T(ctx), id, cg.Arguments(0, kConstructMaxArgs.AsInt()));
    }
  }
  if (v == nullptr) {
    v = cg.Construct();
  }
  if (v == nullptr) {
    return false;
  }
  Push(out, cg.Declare(v, Builder::GuessType(v), "o"));
  return true;
}

GEN(TypeCheck) {
  Random& r = ctx.rng();
  Expression* v = nullptr;
  switch (r.Index(4)) {
    case 0:
      v = mk::Unary(T(ctx), UnaryOp::kTypeof, cg.Value(JsType::kUnknown, 1));
      break;
    case 1: {
      Expression* ctor = cg.Variable(JsType::kClass);
      if (ctor == nullptr) {
        ctor = mk::Id(T(ctx), r.Pick({"Object", "Array", "Function", "Error",
                                      "Map", "Set"}));
      }
      v = mk::Binary(T(ctx), BinaryOp::kInstanceof,
                     cg.Value(JsType::kUnknown, 1), ctor);
      break;
    }
    case 2:
      v = mk::Binary(T(ctx), BinaryOp::kIn,
                     r.Coin() ? cg.Key() : mk::Str(T(ctx), cg.PropertyName()),
                     cg.ObjectLike());
      break;
    default: {
      Expression* type_str =
          mk::Str(T(ctx), r.Pick({"undefined", "object", "boolean", "number",
                                  "bigint", "string", "symbol", "function"}));
      v = mk::Binary(
          T(ctx), CodeGen::RandomComparisonOp(r),
          mk::Unary(T(ctx), UnaryOp::kTypeof, cg.Value(JsType::kUnknown, 1)),
          type_str);
      break;
    }
  }
  Push(out, cg.Declare(v, Builder::GuessType(v), "b"));
  return true;
}

GEN(SpreadCall) {
  Identifier* arr = cg.Variable(JsType::kArray);
  if (arr == nullptr) {
    return false;
  }
  Expression* callee = cg.Callable();
  std::vector<Expression*> args;
  if (ctx.rng().Coin()) {
    args.push_back(cg.Value(JsType::kUnknown, 1));
  }
  args.push_back(mk::Spread(T(ctx), arr));
  Push(out,
       cg.Declare(mk::Call(T(ctx), callee, std::move(args)), JsType::kUnknown));
  return true;
}

// --- control flow
// ---------------------------------------------------------------

REDLINE_TUNABLE(kCountedLoopMaxStatements,
                "generators.CountedLoop.max_statements", 4, 1, 64,
                "most statements in the loop body");
REDLINE_TUNABLE(kCountedLoopLiteralCountChance,
                "generators.CountedLoop.literal_count_chance", 0.8, 0, 1,
                "probability that the loop bound is a literal count rather "
                "than a computed number");

GEN(CountedLoop) {
  Random& r = ctx.rng();
  std::string_view counter = ctx.builder.FreshName("i");
  GenContext inner = ctx.Inner(true, false);
  inner.Declare(counter, JsType::kNumber);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kCountedLoopMaxStatements.AsInt() - 1));
  Expression* count = r.Chance(kCountedLoopLiteralCountChance())
                          ? mk::Num(T(ctx), LoopCount(r))
                          : cg.Value(JsType::kNumber, 1);
  Push(out, mk::CountedLoop(T(ctx), counter, count,
                            mk::Block(T(ctx), std::move(body))));
  return true;
}

REDLINE_TUNABLE(kWhileLoopMaxStatements, "generators.WhileLoop.max_statements",
                4, 1, 64, "most statements in the loop body");
REDLINE_TUNABLE(kWhileLoopDoWhileChance, "generators.WhileLoop.do_while_chance",
                0.25, 0, 1, "probability of do-while rather than while");

GEN(WhileLoop) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  // Bounded: let n = 0; while (cond && n++ < N) { ... }
  std::string_view counter = ctx.builder.FreshName("n");
  Push(out, mk::Let(t, counter, mk::Num(t, 0)));
  GenContext inner = ctx.Inner(true, false);
  inner.Declare(counter, JsType::kNumber);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kWhileLoopMaxStatements.AsInt() - 1));
  Expression* bound =
      mk::Binary(t, BinaryOp::kLessThan,
                 mk::Update(t, UpdateOp::kIncrement, false, mk::Id(t, counter)),
                 mk::Num(t, LoopCount(r)));
  Expression* test =
      r.Coin() ? bound
               : mk::Logical(t, LogicalOp::kAnd, cg.Condition(1), bound);
  if (r.Chance(kWhileLoopDoWhileChance())) {
    Push(out, mk::DoWhile(t, mk::Block(t, std::move(body)), test));
  } else {
    Push(out, mk::While(t, test, mk::Block(t, std::move(body))));
  }
  return true;
}

REDLINE_TUNABLE(kForOfMaxStatements, "generators.ForOf.max_statements", 3, 1,
                64, "most statements in the loop body");
REDLINE_TUNABLE(kForOfAwaitChance, "generators.ForOf.await_chance", 0.2, 0, 1,
                "probability of for await when in an async context");
REDLINE_TUNABLE(kForOfConstChance, "generators.ForOf.const_chance", 0.7, 0, 1,
                "probability that the loop variable is const rather than let");

GEN(ForOf) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* iterable = nullptr;
  switch (r.Index(6)) {
    case 0:
      iterable = cg.Variable(JsType::kArray);
      break;
    case 1:
      iterable = cg.Variable(JsType::kString);
      break;
    case 2:
      iterable = cg.Variable(r.Coin() ? JsType::kMap : JsType::kSet);
      break;
    case 3:
      iterable = cg.Variable(JsType::kTypedArray);
      break;
    case 4:
      iterable =
          mk::CallMethod(t, mk::Id(t, "Object"), r.Coin() ? "keys" : "entries",
                         {cg.ObjectLike()});
      break;
    default:
      break;
  }
  if (iterable == nullptr) {
    iterable = cg.ArrayLiteral(1);
  }
  std::string_view name = ctx.builder.FreshName("e");
  GenContext inner = ctx.Inner(true, false);
  inner.Declare(name, JsType::kUnknown);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kForOfMaxStatements.AsInt() - 1));
  bool is_await = ctx.syntax.in_async && !ctx.syntax.in_static_block &&
                  r.Chance(kForOfAwaitChance());
  Push(out, mk::ForOf(t,
                      r.Chance(kForOfConstChance()) ? VariableKind::kConst
                                                    : VariableKind::kLet,
                      name, iterable, mk::Block(t, std::move(body)), is_await));
  return true;
}

REDLINE_TUNABLE(kForInMaxStatements, "generators.ForIn.max_statements", 3, 1,
                64, "most statements in the loop body");
REDLINE_TUNABLE(kForInTouchPropertyChance,
                "generators.ForIn.touch_property_chance", 0.5, 0, 1,
                "probability that the body starts by loading obj[k] when the "
                "object is a variable");

GEN(ForIn) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view name = ctx.builder.FreshName("k");
  Expression* object = cg.ObjectLike();
  GenContext inner = ctx.Inner(true, false);
  inner.Declare(name, JsType::kString);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kForInMaxStatements.AsInt() - 1));
  if (r.Chance(kForInTouchPropertyChance()) && object->Is<Identifier>()) {
    // Touch the property inside the loop.
    body.insert(
        body.begin(),
        icg.Declare(mk::Index(t, mk::Id(t, object->Cast<Identifier>()->name),
                              mk::Id(t, name)),
                    JsType::kUnknown));
  }
  Push(out, mk::ForIn(t, VariableKind::kLet, name, object,
                      mk::Block(t, std::move(body))));
  return true;
}

REDLINE_TUNABLE(kIfElseElseChance, "generators.IfElse.else_chance", 0.5, 0, 1,
                "probability that the if has an else branch");

GEN(IfElse) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Statements consequent = InnerBody(ctx, cg, false, false);
  Statement* alternate = nullptr;
  if (r.Chance(kIfElseElseChance())) {
    Statements alt = InnerBody(ctx, cg, false, false);
    alternate = mk::Block(t, std::move(alt));
  }
  Push(out, mk::If(t, cg.Condition(0), mk::Block(t, std::move(consequent)),
                   alternate));
  return true;
}

REDLINE_TUNABLE(kSwitchMaxCases, "generators.Switch.max_cases", 4, 1, 64,
                "most case clauses, not counting default");
REDLINE_TUNABLE(kSwitchCaseMaxStatements,
                "generators.Switch.case_max_statements", 3, 1, 64,
                "most statements in a case or default body");
REDLINE_TUNABLE(kSwitchBreakChance, "generators.Switch.break_chance", 0.7, 0, 1,
                "probability that a case ends with break");
REDLINE_TUNABLE(kSwitchIntegerLabelChance,
                "generators.Switch.integer_label_chance", 0.6, 0, 1,
                "probability that a case label is an integer literal rather "
                "than an arbitrary value");
REDLINE_TUNABLE(kSwitchDefaultChance, "generators.Switch.default_chance", 0.5,
                0, 1, "probability that the switch has a default clause");

GEN(Switch) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* discriminant =
      cg.Value(r.Coin() ? JsType::kNumber : JsType::kUnknown, 0);
  std::vector<SwitchCase*> cases;
  int n = 1 + r.SmallCount(kSwitchMaxCases.AsInt() - 1);
  for (int i = 0; i < n; i++) {
    GenContext inner = ctx.Inner(false, true);
    CodeGen icg(inner);
    Statements body =
        icg.Body(inner, 1 + r.SmallCount(kSwitchCaseMaxStatements.AsInt() - 1));
    if (r.Chance(kSwitchBreakChance())) {
      body.push_back(mk::Break(t));
    }
    cases.push_back(mk::Case(t,
                             r.Chance(kSwitchIntegerLabelChance())
                                 ? cg.Integer()
                                 : cg.Value(JsType::kUnknown, 1),
                             std::move(body)));
  }
  if (r.Chance(kSwitchDefaultChance())) {
    GenContext inner = ctx.Inner(false, true);
    CodeGen icg(inner);
    Statements body =
        icg.Body(inner, 1 + r.SmallCount(kSwitchCaseMaxStatements.AsInt() - 1));
    cases.insert(
        cases.begin() + static_cast<std::ptrdiff_t>(r.Index(cases.size() + 1)),
        mk::Case(t, nullptr, std::move(body)));
  }
  Push(out, mk::Switch(t, discriminant, std::move(cases)));
  return true;
}

REDLINE_TUNABLE(kTryCatchThrowChance, "generators.TryCatch.throw_chance", 0.3,
                0, 1, "probability that the try block ends with a throw");
REDLINE_TUNABLE(kTryCatchParamChance, "generators.TryCatch.param_chance", 0.8,
                0, 1, "probability that the catch clause binds the exception");
REDLINE_TUNABLE(kTryCatchHandlerMaxStatements,
                "generators.TryCatch.handler_max_statements", 2, 0, 64,
                "most statements in the catch handler");
REDLINE_TUNABLE(kTryCatchFinallyChance, "generators.TryCatch.finally_chance",
                0.3, 0, 1, "probability of a finally block");
REDLINE_TUNABLE(kTryCatchFinallyMaxStatements,
                "generators.TryCatch.finally_max_statements", 2, 1, 64,
                "most statements in the finally block");

GEN(TryCatch) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Statements block = InnerBody(ctx, cg, false, false);
  if (r.Chance(kTryCatchThrowChance())) {
    block.push_back(mk::Throw(t, cg.Value(JsType::kUnknown, 1)));
  }
  std::string_view param = r.Chance(kTryCatchParamChance())
                               ? ctx.builder.FreshName("e")
                               : std::string_view();
  GenContext inner = ctx.Inner(false, false);
  if (!param.empty()) {
    inner.Declare(param, JsType::kError);
  }
  CodeGen icg(inner);
  Statements handler =
      icg.Body(inner, r.SmallCount(kTryCatchHandlerMaxStatements.AsInt()));
  if (r.Chance(kTryCatchFinallyChance())) {
    Statements finalizer =
        InnerBody(ctx, cg, false, false, kTryCatchFinallyMaxStatements.AsInt());
    Push(out,
         mk::Try(t, std::move(block), param, std::move(handler), &finalizer));
  } else {
    Push(out, mk::Try(t, std::move(block), param, std::move(handler)));
  }
  return true;
}

GEN(Throw) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* value = nullptr;
  if (r.Coin()) {
    value = mk::New(
        t,
        mk::Id(t, r.Pick({"Error", "TypeError", "RangeError", "SyntaxError"})),
        {cg.String()});
  } else {
    value = cg.Value(JsType::kUnknown, 1);
  }
  // Only inside a try so that execution continues.
  Statements block = {mk::Throw(t, value)};
  std::string_view param = ctx.builder.FreshName("e");
  Push(out, mk::Try(t, std::move(block), param, {}));
  return true;
}

REDLINE_TUNABLE(kLabeledBlockMaxStatements,
                "generators.LabeledBlock.max_statements", 3, 1, 64,
                "most statements in the labeled block");

GEN(LabeledBlock) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view label = ctx.builder.FreshName("L");
  GenContext inner = ctx.Inner(false, false);
  inner.syntax.labels.push_back(label);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kLabeledBlockMaxStatements.AsInt() - 1));
  if (r.Coin()) {
    body.insert(
        body.begin() + static_cast<std::ptrdiff_t>(r.Index(body.size() + 1)),
        mk::If(t, icg.Condition(1), mk::Break(t, label)));
  } else {
    body.push_back(mk::Break(t, label));
  }
  Push(out, mk::Labeled(t, label, mk::Block(t, std::move(body))));
  return true;
}

REDLINE_TUNABLE(kReturnValueChance, "generators.Return.value_chance", 0.8, 0, 1,
                "probability that the return has a value");
REDLINE_TUNABLE(kReturnConditionalChance,
                "generators.Return.conditional_chance", 0.7, 0, 1,
                "probability that the return is guarded by an if (keeping the "
                "rest of the function reachable)");

GEN(Return) {
  Random& r = ctx.rng();
  // Conditional returns keep the rest of the function reachable.
  Statement* ret = mk::Return(T(ctx), r.Chance(kReturnValueChance())
                                          ? cg.Value(JsType::kUnknown, 0)
                                          : nullptr);
  if (r.Chance(kReturnConditionalChance())) {
    Push(out, mk::If(T(ctx), cg.Condition(0), mk::Block(T(ctx), {ret})));
  } else {
    Push(out, ret);
  }
  return true;
}

GEN(BreakContinue) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Statement* s = nullptr;
  if (ctx.syntax.in_loop && r.Coin()) {
    s = mk::Continue(t);
  } else {
    s = mk::Break(t);
  }
  Push(out, mk::If(t, cg.Condition(0), mk::Block(t, {s})));
  return true;
}

// --- JIT
// ------------------------------------------------------------------------

// Picks a function to compile: a visible function variable, or a fresh one.
Identifier* FunctionForJit(GenContext& ctx, CodeGen& cg, Statements* out) {
  if (Identifier* f = cg.Variable(JsType::kFunction)) {
    return f;
  }
  Statements decl;
  if (!GenFunctionDecl(ctx, cg, &decl)) {
    return nullptr;
  }
  out->insert(out->end(), decl.begin(), decl.end());
  // The declaration registered the name as the newest visible function.
  for (const Variable& v : ctx.visible) {
    if (v.type == JsType::kFunction) {
      return mk::Id(T(ctx), v.name);
    }
  }
  return nullptr;
}

REDLINE_TUNABLE(kJitOptimizeMaxWarmupCalls,
                "generators.JitOptimize.max_warmup_calls", 4, 1, 64,
                "most calls between %PrepareFunctionForOptimization and the "
                "optimization request");
REDLINE_TUNABLE(kJitOptimizeTurbofanChance,
                "generators.JitOptimize.turbofan_chance", 0.6, 0, 1,
                "probability of %OptimizeFunctionOnNextCall rather than "
                "%OptimizeMaglevOnNextCall");
REDLINE_TUNABLE(kJitOptimizeExtraCallChance,
                "generators.JitOptimize.extra_call_chance", 0.3, 0, 1,
                "probability of a second call of the optimized function");
REDLINE_TUNABLE(
    kJitOptimizeDeoptChance, "generators.JitOptimize.deopt_chance", 0.2, 0, 1,
    "probability of a %DeoptimizeFunction followed by another call");
REDLINE_TUNABLE(kJitOptimizeConstantCallerChance,
                "generators.JitOptimize.constant_caller_chance", 0.2, 0, 1,
                "probability that the optimized function is a new caller "
                "passing literal arguments to the warmed-up function");

GEN(JitOptimize) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Identifier* f = FunctionForJit(ctx, cg, out);
  if (f == nullptr) {
    return false;
  }
  std::string_view name = f->name;
  std::string_view target_name = name;

  bool constant_caller = r.Chance(kJitOptimizeConstantCallerChance());
  if (constant_caller) {
    target_name = ctx.builder.FreshName("caller");
    std::vector<Expression*> args;
    int args_count = 1 + static_cast<int>(r.Index(3));
    args.reserve(static_cast<size_t>(args_count));
    for (int i = 0; i < args_count; i++) {
      args.push_back(cg.Literal());
    }
    Statements caller_body;
    caller_body.push_back(
        mk::Return(t, mk::Call(t, mk::Id(t, name), std::move(args))));
    Push(out, mk::FunctionDecl(t, target_name, {}, std::move(caller_body),
                               false, false));
    ctx.Declare(target_name, JsType::kFunction);
  }

  Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                               {mk::Id(t, target_name)})));
  int warmup = 1 + r.SmallCount(kJitOptimizeMaxWarmupCalls.AsInt() - 1);
  for (int i = 0; i < warmup; i++) {
    Push(out, cg.Stmt(cg.CallWithArguments(mk::Id(t, name))));
  }
  const char* tier = r.Chance(kJitOptimizeTurbofanChance())
                         ? "OptimizeFunctionOnNextCall"
                         : "OptimizeMaglevOnNextCall";
  Push(out, cg.Stmt(mk::Native(t, tier, {mk::Id(t, target_name)})));

  auto call_target = [&] {
    if (constant_caller) {
      return cg.Stmt(mk::Call(t, mk::Id(t, target_name), {}));
    }
    return cg.Stmt(cg.CallWithArguments(mk::Id(t, name)));
  };

  Push(out, call_target());
  if (r.Chance(kJitOptimizeExtraCallChance())) {
    Push(out, call_target());
  }
  if (r.Chance(kJitOptimizeDeoptChance())) {
    Push(out, cg.Stmt(mk::Native(t, "DeoptimizeFunction",
                                 {mk::Id(t, target_name)})));
    Push(out, call_target());
  }
  return true;
}

GEN(JitBaseline) {
  Tree* t = T(ctx);
  Identifier* f = FunctionForJit(ctx, cg, out);
  if (f == nullptr) {
    return false;
  }
  Push(out, cg.Stmt(mk::Native(t, "CompileBaseline", {f})));
  Push(out, cg.Stmt(cg.CallWithArguments(mk::Id(t, f->name))));
  return true;
}

REDLINE_TUNABLE(kDeoptDeoptimizeFunctionChance,
                "generators.Deopt.deoptimize_function_chance", 0.8, 0, 1,
                "probability of %DeoptimizeFunction(f) rather than "
                "%NeverOptimizeFunction(f)");

GEN(Deopt) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (r.Coin()) {
    Push(out, cg.Stmt(mk::Native(t, "DeoptimizeNow", {})));
    return true;
  }
  Identifier* f = cg.Variable(JsType::kFunction);
  if (f == nullptr) {
    return false;
  }
  Push(out, cg.Stmt(mk::Native(t,
                               r.Chance(kDeoptDeoptimizeFunctionChance())
                                   ? "DeoptimizeFunction"
                                   : "NeverOptimizeFunction",
                               {f})));
  return true;
}

REDLINE_TUNABLE(kOsrLoopMaxStatements, "generators.OsrLoop.max_statements", 4,
                1, 64, "most statements in the OSR loop body");
REDLINE_TUNABLE(
    kOsrLoopMinTriggerIteration, "generators.OsrLoop.min_trigger_iteration", 1,
    0, 256,
    "earliest loop iteration at which %OptimizeOsr/%BaselineOsr is requested");
REDLINE_TUNABLE(
    kOsrLoopMaxTriggerIteration, "generators.OsrLoop.max_trigger_iteration", 50,
    1, 256,
    "latest loop iteration at which %OptimizeOsr/%BaselineOsr is requested");

GEN(OsrLoop) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view counter = ctx.builder.FreshName("i");
  GenContext inner = ctx.Inner(true, false);
  inner.Declare(counter, JsType::kNumber);
  CodeGen icg(inner);
  Statements body =
      icg.Body(inner, 1 + r.SmallCount(kOsrLoopMaxStatements.AsInt() - 1));
  Statement* osr =
      icg.Stmt(mk::Native(t, r.Coin() ? "OptimizeOsr" : "BaselineOsr", {}));
  body.insert(
      body.begin(),
      mk::If(t,
             mk::Binary(t, BinaryOp::kStrictEq, mk::Id(t, counter),
                        mk::Num(t, static_cast<double>(r.Range(
                                       kOsrLoopMinTriggerIteration.AsInt(),
                                       kOsrLoopMaxTriggerIteration.AsInt())))),
             mk::Block(t, {osr})));
  Push(out, mk::CountedLoop(t, counter,
                            mk::Num(t, r.Pick({5.0, 7.0, 10.0, 20.0, 100.0})),
                            mk::Block(t, std::move(body))));
  return true;
}

REDLINE_TUNABLE(
    kNativeCallDeclareChance, "generators.NativeCall.declare_chance", 0.3, 0, 1,
    "probability that the result of the %Native call is bound to a variable");

GEN(NativeCall) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const NativeFunction& n = r.Pick(ctx.env.natives());
  std::vector<Expression*> args;
  for (int i = 0; i < n.args; i++) {
    Expression* arg = nullptr;
    if (n.arg_type != JsType::kUnknown) {
      arg = cg.Variable(n.arg_type);
    }
    if (arg == nullptr) {
      arg = i == 0 && n.arg_type == JsType::kFunction ? cg.Callable()
                                                      : cg.Value(n.arg_type, 1);
    }
    args.push_back(arg);
  }
  Expression* call = mk::Native(t, n.name, std::move(args));
  if (r.Chance(kNativeCallDeclareChance())) {
    Push(out, cg.Declare(call, JsType::kUnknown));
  } else {
    Push(out, cg.Stmt(call));
  }
  return true;
}

GEN(Gc) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  switch (r.Index(6)) {
    case 0:
      Push(out, cg.Stmt(mk::Call(t, mk::Id(t, "gc"), {})));
      break;
    case 1:
      Push(out,
           cg.Stmt(mk::Call(
               t, mk::Id(t, "gc"),
               {mk::Object(t, {mk::Prop(t, "type", mk::Str(t, "minor"))})})));
      break;
    case 2:
      Push(out, cg.Stmt(mk::Native(t, "SimulateNewspaceFull", {})));
      break;
    case 3: {
      // %PretenureAllocationSite(o); gc(): objects allocated at o's site
      // go to old space from now on, so later stores into them take the
      // write-barrier paths that young objects skip.
      Expression* object = cg.Variable(JsType::kObject);
      if (object == nullptr) {
        object = cg.Variable(JsType::kArray);
      }
      if (object == nullptr) {
        object = cg.ObjectLiteral(1);
      }
      Push(out, cg.Stmt(mk::Native(t, "PretenureAllocationSite", {object})));
      Push(out, cg.Stmt(mk::Call(t, mk::Id(t, "gc"), {})));
      break;
    }
    default:
      Push(out,
           cg.Stmt(mk::Call(t, mk::Id(t, "gc"),
                            {mk::Object(t, {mk::Prop(t, "execution",
                                                     mk::Str(t, "async"))})})));
      break;
  }
  return true;
}

// --- object model games
// ---------------------------------------------------------

REDLINE_TUNABLE(kPrototypeSetNullChance, "generators.PrototypeSet.null_chance",
                0.3, 0, 1, "probability that the new prototype is null");

GEN(PrototypeSet) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* object = cg.ObjectLike();
  Expression* proto = r.Chance(kPrototypeSetNullChance())
                          ? static_cast<Expression*>(mk::Null(t))
                          : cg.ObjectLike();
  if (r.Coin()) {
    Push(out, cg.Stmt(mk::CallMethod(t, mk::Id(t, "Object"), "setPrototypeOf",
                                     {object, proto})));
  } else {
    Push(out,
         cg.Stmt(mk::Assign(t, mk::Member(t, object, "__proto__"), proto)));
  }
  return true;
}

REDLINE_TUNABLE(kDefinePropertyStringKeyChance,
                "generators.DefineProperty.string_key_chance", 0.7, 0, 1,
                "probability that the defined key is a property name string "
                "rather than a computed key");

GEN(DefineProperty) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* object = cg.ObjectLike();
  std::vector<Node*> descriptor;
  if (r.Coin()) {
    GenContext nested = ctx.Nested(true, false, false, false);
    CodeGen inner(nested);
    Statements getter = {mk::Return(t, inner.Value(JsType::kUnknown, 1))};
    descriptor.push_back(
        mk::Prop(t, "get", mk::Function(t, "", {}, std::move(getter))));
    if (r.Coin()) {
      std::string_view p = ctx.builder.FreshName("v");
      Statements setter = {inner.Stmt(
          mk::Assign(t, mk::Member(t, mk::This(t), "x"), mk::Id(t, p)))};
      descriptor.push_back(
          mk::Prop(t, "set", mk::Function(t, "", {p}, std::move(setter))));
    }
  } else {
    descriptor.push_back(mk::Prop(t, "value", cg.Value(JsType::kUnknown, 1)));
    if (r.Coin()) {
      descriptor.push_back(mk::Prop(t, "writable", cg.Boolean()));
    }
  }
  if (r.Coin()) {
    descriptor.push_back(mk::Prop(t, "configurable", cg.Boolean()));
  }
  if (r.Coin()) {
    descriptor.push_back(mk::Prop(t, "enumerable", cg.Boolean()));
  }
  Expression* key = r.Chance(kDefinePropertyStringKeyChance())
                        ? mk::Str(t, cg.PropertyName())
                        : cg.Key();
  Push(out, cg.Stmt(mk::CallMethod(
                t, mk::Id(t, "Object"), "defineProperty",
                {object, key, mk::Object(t, std::move(descriptor))})));
  return true;
}

GEN(ValueOfOverride) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* object = cg.Variable(JsType::kObject);
  if (object == nullptr) {
    object = cg.Variable(JsType::kArray);
  }
  if (object == nullptr) {
    object = cg.ObjectLike();
  }
  GenContext nested = ctx.Nested(true, true, false, false);
  CodeGen inner(nested);
  Expression* fn = mk::ArrowExpr(
      t, {}, inner.Value(r.Coin() ? JsType::kNumber : JsType::kUnknown, 1));
  Expression* target = nullptr;
  switch (r.Index(3)) {
    case 0:
      target = mk::Member(t, object, "valueOf");
      break;
    case 1:
      target = mk::Member(t, object, "toString");
      break;
    default:
      target = mk::Index(t, object,
                         mk::Member(t, mk::Id(t, "Symbol"), "toPrimitive"));
      break;
  }
  Push(out, cg.Stmt(mk::Assign(t, target, fn)));
  // Trigger a conversion.
  if (r.Coin()) {
    Push(out,
         cg.Declare(mk::Binary(t, r.Coin() ? BinaryOp::kAdd : BinaryOp::kMul,
                               object, cg.Number()),
                    JsType::kUnknown));
  }
  return true;
}

REDLINE_TUNABLE(kElementsKindMaxTransitions,
                "generators.ElementsKind.max_transitions", 4, 1, 64,
                "most elements-kind transitions applied to the array");

GEN(ElementsKind) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Identifier* arr = cg.Variable(JsType::kArray);
  if (arr == nullptr) {
    Statement* decl = cg.Declare(cg.ArrayLiteral(1), JsType::kArray, "arr");
    Push(out, decl);
    arr = mk::Id(t, decl->Cast<VariableDeclaration>()
                        ->declarations[0]
                        ->target->Cast<Identifier>()
                        ->name);
  }
  int n = 1 + r.SmallCount(kElementsKindMaxTransitions.AsInt() - 1);
  for (int i = 0; i < n; i++) {
    Identifier* a = mk::Id(t, arr->name);
    switch (r.Index(9)) {
      case 0:
        Push(out, cg.Stmt(mk::CallMethod(t, a, "push", {mk::Num(t, 1.5)})));
        break;
      case 1:
        Push(out, cg.Stmt(mk::CallMethod(t, a, "push", {cg.ObjectLike()})));
        break;
      case 2:
        Push(out, cg.Stmt(mk::Assign(
                      t,
                      mk::Index(t, a,
                                mk::Num(t, r.Pick({10.0, 100.0, 1000.0,
                                                   100000.0, 4294967294.0}))),
                      cg.Value(JsType::kNumber, 1))));
        break;
      case 3:
        Push(out,
             cg.Stmt(mk::Assign(t, mk::Member(t, a, "length"), cg.Integer())));
        break;
      case 4:
        Push(out, cg.Stmt(mk::CallMethod(
                      t, a, r.Pick({"pop", "shift", "reverse", "sort"}), {})));
        break;
      case 5:
        Push(out,
             cg.Stmt(mk::Assign(
                 t, mk::Index(t, a, mk::Num(t, 0)),
                 cg.Value(r.Coin() ? JsType::kString : JsType::kNumber, 1))));
        break;
      case 6:
        Push(out, cg.Stmt(mk::Unary(t, UnaryOp::kDelete,
                                    mk::Index(t, a, cg.Integer()))));
        break;
      case 7:
        Push(out, cg.Stmt(mk::CallMethod(t, a, "unshift",
                                         {cg.Value(JsType::kUnknown, 1)})));
        break;
      default:
        Push(out, cg.Stmt(mk::Native(t, "NormalizeElements", {a})));
        break;
    }
  }
  return true;
}

GEN(FreezeSeal) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* object = cg.ObjectLike();
  Push(out, cg.Stmt(mk::CallMethod(
                t, mk::Id(t, "Object"),
                r.Pick({"freeze", "seal", "preventExtensions"}), {object})));
  return true;
}

REDLINE_TUNABLE(kProxyDeclMaxTraps, "generators.ProxyDecl.max_traps", 3, 1, 64,
                "most traps in the proxy handler");
REDLINE_TUNABLE(kProxyDeclTrapMaxStatements,
                "generators.ProxyDecl.trap_max_statements", 2, 0, 64,
                "most statements in a trap body before its return");

GEN(ProxyDecl) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* target = cg.ObjectLike();
  std::vector<Node*> handler;
  int n = 1 + r.SmallCount(kProxyDeclMaxTraps.AsInt() - 1);
  for (int i = 0; i < n; i++) {
    const char* trap = r.Pick({"get", "set", "has", "deleteProperty", "ownKeys",
                               "getOwnPropertyDescriptor", "apply", "construct",
                               "getPrototypeOf", "defineProperty"});
    std::string_view p1 = ctx.builder.FreshName("t");
    std::string_view p2 = ctx.builder.FreshName("k");
    GenContext nested = ctx.Nested(true, false, false, false);
    nested.Declare(p1, JsType::kObject);
    nested.Declare(p2, JsType::kUnknown);
    CodeGen inner(nested);
    Statements body =
        inner.Body(nested, r.SmallCount(kProxyDeclTrapMaxStatements.AsInt()));
    Expression* result = nullptr;
    std::string_view trap_name = trap;
    if (trap_name == "set" || trap_name == "has" ||
        trap_name == "deleteProperty" || trap_name == "defineProperty") {
      result = mk::Bool(t, true);
    } else if (trap_name == "ownKeys") {
      result =
          mk::CallMethod(t, mk::Id(t, "Reflect"), "ownKeys", {mk::Id(t, p1)});
    } else if (trap_name == "getOwnPropertyDescriptor") {
      result =
          mk::CallMethod(t, mk::Id(t, "Reflect"), "getOwnPropertyDescriptor",
                         {mk::Id(t, p1), mk::Id(t, p2)});
    } else if (trap_name == "getPrototypeOf") {
      result = mk::CallMethod(t, mk::Id(t, "Reflect"), "getPrototypeOf",
                              {mk::Id(t, p1)});
    } else if (trap_name == "get") {
      result = r.Coin() ? static_cast<Expression*>(
                              mk::CallMethod(t, mk::Id(t, "Reflect"), "get",
                                             {mk::Id(t, p1), mk::Id(t, p2)}))
                        : inner.Value(JsType::kUnknown, 1);
    } else {
      result = inner.Value(JsType::kUnknown, 1);
    }
    body.push_back(mk::Return(t, result));
    handler.push_back(
        mk::Method(t, trap, mk::Function(t, "", {p1, p2}, std::move(body))));
  }
  Push(out, cg.Declare(mk::New(t, mk::Id(t, "Proxy"),
                               {target, mk::Object(t, std::move(handler))}),
                       JsType::kObject, "proxy"));
  return true;
}

REDLINE_TUNABLE(kWithMaxStatements, "generators.With.max_statements", 2, 1, 64,
                "most statements in the with body");

GEN(With) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  WithStatement* w = t->New<WithStatement>();
  w->object = cg.ObjectLike();
  Statements body =
      InnerBody(ctx, cg, false, false, kWithMaxStatements.AsInt());
  (void)r;
  w->body = mk::Block(t, std::move(body));
  Push(out, w);
  return true;
}

REDLINE_TUNABLE(kShapeChangeMaxChanges, "generators.ShapeChange.max_changes", 4,
                1, 64, "most property additions/deletions on the object");
REDLINE_TUNABLE(
    kShapeChangeAddChance, "generators.ShapeChange.add_chance", 0.7, 0, 1,
    "probability that a change stores a property rather than deleting one");

GEN(ShapeChange) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Identifier* object = cg.Variable(JsType::kObject);
  if (object == nullptr) {
    Statement* decl = cg.Declare(cg.ObjectLiteral(1), JsType::kObject, "obj");
    Push(out, decl);
    object = mk::Id(t, decl->Cast<VariableDeclaration>()
                           ->declarations[0]
                           ->target->Cast<Identifier>()
                           ->name);
  }
  int n = 1 + r.SmallCount(kShapeChangeMaxChanges.AsInt() - 1);
  for (int i = 0; i < n; i++) {
    Identifier* o = mk::Id(t, object->name);
    if (r.Chance(kShapeChangeAddChance())) {
      std::string_view name = cg.PropertyName();
      if (name == "__proto__") {
        name = "x";
      }
      Push(out, cg.Stmt(mk::Assign(t, cg.Property(o, name),
                                   cg.Value(JsType::kUnknown, 1))));
    } else {
      Push(out, cg.Stmt(mk::Unary(t, UnaryOp::kDelete, cg.Property(o))));
    }
  }
  return true;
}

REDLINE_TUNABLE(kIterationArrayTypeChance,
                "generators.Iteration.array_type_chance", 0.75, 0, 1,
                "probability that the result variable is declared as an array "
                "rather than an object");

GEN(Iteration) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* source = cg.ObjectLike();
  Expression* v = nullptr;
  switch (r.Index(4)) {
    case 0:
      v = mk::CallMethod(t, mk::Id(t, "Array"), "from", {source});
      break;
    case 1:
      v = mk::Array(t, {mk::Spread(t, source)});
      break;
    case 2:
      v = mk::Object(t, {mk::Spread(t, source)});
      break;
    default:
      v = mk::CallMethod(
          t, mk::Id(t, "Object"),
          r.Pick({"keys", "values", "entries", "getOwnPropertyNames"}),
          {source});
      break;
  }
  Push(out, cg.Declare(v,
                       r.Chance(kIterationArrayTypeChance()) ? JsType::kArray
                                                             : JsType::kObject,
                       "arr"));
  return true;
}

// --- discovered APIs and builtin plumbing
// -----------------------------------------

// "Intl.NumberFormat" -> Intl.NumberFormat (a member chain on identifiers).
Expression* PathExpression(Tree* t, std::string_view path) {
  size_t dot = path.find('.');
  Expression* e = mk::Id(t, path.substr(0, dot));
  while (dot != std::string_view::npos) {
    size_t next = path.find('.', dot + 1);
    std::string_view part = path.substr(dot + 1, next == std::string_view::npos
                                                     ? std::string_view::npos
                                                     : next - dot - 1);
    e = IsIdentifierName(part)
            ? static_cast<Expression*>(mk::Member(t, e, part))
            : static_cast<Expression*>(mk::Index(t, e, mk::Str(t, part)));
    dot = next;
  }
  return e;
}

Expression* MemberOf(Tree* t, Expression* object, std::string_view name) {
  if (IsIdentifierName(name)) {
    return mk::Member(t, object, name);
  }
  return mk::Index(t, object, mk::Str(t, name));
}

// The name declared by the last cg.Declare() (it registers the variable at
// the front of the visible list).
std::string_view LastDeclared(GenContext& ctx) {
  return ctx.visible.front().name;
}

// A value produced by something the target reported about itself: an
// instance of a discovered class, or the result of a discovered function or
// namespace static. nullptr if nothing was discovered. Such values are the
// embedder's own object kinds (API objects, interceptors, access-checked
// objects, ...), which plain generation never produces. `*source` receives
// the API the value came from.
Expression* DiscoveredValue(GenContext& ctx, CodeGen& cg,
                            const DiscoveredApi** source = nullptr) {
  const std::vector<DiscoveredApi>& apis = ctx.env.discovered();
  if (apis.empty()) {
    return nullptr;
  }
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  // Draw by surface: a namespace counts once per static it offers, a
  // function or constructor once, so every producer the target reported
  // is equally likely regardless of how the embedder grouped them.
  size_t total = 0;
  for (const DiscoveredApi& a : apis) {
    total += a.kind == DiscoveredApi::Kind::kNamespace ? a.statics.size() : 1;
  }
  if (total == 0) {
    return nullptr;
  }
  size_t pick = r.Index(total);
  const DiscoveredApi* found = nullptr;
  for (const DiscoveredApi& a : apis) {
    size_t weight =
        a.kind == DiscoveredApi::Kind::kNamespace ? a.statics.size() : 1;
    if (pick < weight) {
      found = &a;
      break;
    }
    pick -= weight;
  }
  if (found == nullptr) {
    return nullptr;  // not reachable: the weights sum to `total`
  }
  const DiscoveredApi& api = *found;
  if (source != nullptr) {
    *source = &api;
  }
  switch (api.kind) {
    case DiscoveredApi::Kind::kConstructor:
      return mk::New(t, PathExpression(t, api.path),
                     cg.Arguments(0, api.arity));
    case DiscoveredApi::Kind::kFunction:
      return mk::Call(t, PathExpression(t, api.path),
                      cg.Arguments(0, api.arity));
    case DiscoveredApi::Kind::kNamespace:
      if (api.statics.empty()) {
        return nullptr;
      }
      const DiscoveredMember& m = r.Pick(api.statics);
      return mk::Call(t, MemberOf(t, PathExpression(t, api.path), m.name),
                      cg.Arguments(0, m.arity));
  }
  return nullptr;
}

// A discovered function or namespace static called with `value` as its
// first argument (the rest random): hands an object the program built back
// to the embedder's API. With `preferred` (the API the value came from) the
// function is taken from that API first: the functions of one namespace
// usually operate on each other's objects. nullptr if there is no such
// function.
Expression* DiscoveredCallOn(GenContext& ctx, CodeGen& cg, Expression* value,
                             const DiscoveredApi* preferred = nullptr) {
  const std::vector<DiscoveredApi>& apis = ctx.env.discovered();
  if (apis.empty()) {
    return nullptr;
  }
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  for (int attempt = 0; attempt < 8; attempt++) {
    const DiscoveredApi& api = preferred != nullptr && attempt < 4
                                   ? *preferred
                                   : apis[r.Index(apis.size())];
    Expression* callee = nullptr;
    int arity = 0;
    if (api.kind == DiscoveredApi::Kind::kFunction &&
        (api.statics.empty() || r.Coin())) {
      callee = PathExpression(t, api.path);
      arity = api.arity;
    } else if (!api.statics.empty()) {
      const DiscoveredMember& m = r.Pick(api.statics);
      callee = MemberOf(t, PathExpression(t, api.path), m.name);
      arity = m.arity;
    }
    if (callee == nullptr || arity < 1) {
      continue;
    }
    std::vector<Expression*> args = cg.Arguments(0, arity - 1);
    args.insert(args.begin(), value);
    return mk::Call(t, callee, std::move(args));
  }
  return nullptr;
}

// Signatures are unknown, so most calls are guarded; the unguarded
// minority keeps exceptions (and their handlers) in the picture.
REDLINE_TUNABLE(kDiscoveredApiGuardChance,
                "generators.DiscoveredApi.guard_chance", 0.8, 0, 1,
                "probability that a discovered-API construction, call or "
                "getter load is wrapped in a try/catch");
REDLINE_TUNABLE(kDiscoveredApiConstructorStaticChance,
                "generators.DiscoveredApi.constructor_static_chance", 0.25, 0,
                1,
                "probability of calling a static of a discovered constructor "
                "instead of constructing an instance");
REDLINE_TUNABLE(kDiscoveredApiMethodChance,
                "generators.DiscoveredApi.method_chance", 0.85, 0, 1,
                "probability of calling a prototype method on the new instance "
                "rather than loading a getter");
REDLINE_TUNABLE(kDiscoveredApiFunctionStaticChance,
                "generators.DiscoveredApi.function_static_chance", 0.3, 0, 1,
                "probability of calling a static of a discovered function "
                "instead of the function itself");
REDLINE_TUNABLE(kDiscoveredApiNamespaceStaticChance,
                "generators.DiscoveredApi.namespace_static_chance", 0.8, 0, 1,
                "probability of calling a static of a discovered namespace "
                "rather than loading one of its getters");

// Uses something the target itself reported to exist (see
// Environment::IntrospectionScript): constructs a discovered class and calls
// one of its prototype methods or loads a getter, calls a static method of a
// namespace/class, or calls a discovered function. The result variables are
// untyped; the Explore mutator takes it from there.
GEN(DiscoveredApi) {
  const std::vector<DiscoveredApi>& apis = ctx.env.discovered();
  if (apis.empty()) {
    return false;
  }
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  const DiscoveredApi& api = apis[r.Index(apis.size())];
  auto call_static = [&](const DiscoveredMember& m) {
    cg.DeclareMaybeGuarded(
        out,
        mk::Call(t, MemberOf(t, PathExpression(t, api.path), m.name),
                 cg.Arguments(0, m.arity)),
        JsType::kUnknown, "r", kDiscoveredApiGuardChance());
  };
  switch (api.kind) {
    case DiscoveredApi::Kind::kConstructor: {
      if (!api.statics.empty() &&
          r.Chance(kDiscoveredApiConstructorStaticChance())) {
        call_static(r.Pick(api.statics));
        return true;
      }
      Expression* value =
          mk::New(t, PathExpression(t, api.path), cg.Arguments(0, api.arity));
      cg.DeclareMaybeGuarded(out, value, JsType::kObject, "o",
                             kDiscoveredApiGuardChance());
      std::string_view name = LastDeclared(ctx);
      if (!api.methods.empty() && r.Chance(kDiscoveredApiMethodChance())) {
        const DiscoveredMember& m = r.Pick(api.methods);
        cg.DeclareMaybeGuarded(out,
                               mk::Call(t, MemberOf(t, mk::Id(t, name), m.name),
                                        cg.Arguments(0, m.arity)),
                               JsType::kUnknown, "r",
                               kDiscoveredApiGuardChance());
      } else if (!api.getters.empty()) {
        cg.DeclareMaybeGuarded(
            out, MemberOf(t, mk::Id(t, name), r.Pick(api.getters).name),
            JsType::kUnknown, "r", kDiscoveredApiGuardChance());
      }
      return true;
    }
    case DiscoveredApi::Kind::kFunction:
      if (!api.statics.empty() &&
          r.Chance(kDiscoveredApiFunctionStaticChance())) {
        call_static(r.Pick(api.statics));
        return true;
      }
      cg.DeclareMaybeGuarded(
          out,
          mk::Call(t, PathExpression(t, api.path), cg.Arguments(0, api.arity)),
          JsType::kUnknown, "r", kDiscoveredApiGuardChance());
      return true;
    case DiscoveredApi::Kind::kNamespace:
      if (!api.statics.empty() &&
          (api.getters.empty() ||
           r.Chance(kDiscoveredApiNamespaceStaticChance()))) {
        call_static(r.Pick(api.statics));
        return true;
      }
      if (api.getters.empty()) {
        return false;
      }
      cg.DeclareMaybeGuarded(
          out,
          MemberOf(t, PathExpression(t, api.path), r.Pick(api.getters).name),
          JsType::kUnknown, "r", kDiscoveredApiGuardChance());
      return true;
  }
  return false;
}

REDLINE_TUNABLE(kPrototypeMethodCallObjectReceiverChance,
                "generators.PrototypeMethodCall.object_receiver_chance", 0.7, 0,
                1,
                "probability that the foreign receiver is an object-like value "
                "rather than any value");
REDLINE_TUNABLE(
    kPrototypeMethodCallCallChance,
    "generators.PrototypeMethodCall.call_chance", 0.7, 0, 1,
    "probability of invoking the method with .call rather than .apply");
REDLINE_TUNABLE(kPrototypeMethodCallGuardChance,
                "generators.PrototypeMethodCall.guard_chance", 0.8, 0, 1,
                "probability that the invocation is wrapped in a try/catch");

// A builtin prototype method applied to an unrelated receiver:
// `Array.prototype.map.call(obj, f)`, `String.prototype.at.call(123, 0)`.
GEN(PrototypeMethodCall) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  struct Entry {
    const char* ctor;
    JsType type;
  };
  static const Entry kEntries[] = {{"Array", JsType::kArray},
                                   {"String", JsType::kString},
                                   {"Object", JsType::kObject},
                                   {"Function", JsType::kFunction},
                                   {"Map", JsType::kMap},
                                   {"Set", JsType::kSet},
                                   {"Promise", JsType::kPromise},
                                   {"RegExp", JsType::kRegExp},
                                   {"Date", JsType::kDate},
                                   {"Number", JsType::kNumber},
                                   {"Uint8Array", JsType::kTypedArray}};
  const Entry& e = kEntries[r.Index(sizeof(kEntries) / sizeof(kEntries[0]))];
  const BuiltinType* bt = ctx.env.ForType(e.type);
  if (bt == nullptr || bt->methods.empty()) {
    return false;
  }
  const BuiltinFunction& m = r.Pick(bt->methods);
  Expression* method =
      mk::Member(t, mk::Member(t, mk::Id(t, e.ctor), "prototype"), m.name);
  Expression* receiver = r.Chance(kPrototypeMethodCallObjectReceiverChance())
                             ? cg.ObjectLike()
                             : cg.Value(JsType::kUnknown, 1);
  std::vector<Expression*> args = cg.Arguments(m);
  Expression* call;
  if (r.Chance(kPrototypeMethodCallCallChance())) {
    args.insert(args.begin(), receiver);
    call = mk::CallMethod(t, method, "call", std::move(args));
  } else {
    call = mk::CallMethod(t, method, "apply",
                          {receiver, mk::Array(t, std::move(args))});
  }
  cg.DeclareMaybeGuarded(out, call, m.result, "r",
                         kPrototypeMethodCallGuardChance());
  return true;
}

REDLINE_TUNABLE(kUnboundMethodCallMaxArgs,
                "generators.UnboundMethodCall.max_args", 2, 0, 16,
                "most arguments when the method is a generic name rather than "
                "a known builtin");
REDLINE_TUNABLE(kUnboundMethodCallBuiltinChance,
                "generators.UnboundMethodCall.builtin_chance", 0.8, 0, 1,
                "probability of taking a builtin method of the object's type "
                "rather than a generic method name");
REDLINE_TUNABLE(kUnboundMethodCallMethodGuardChance,
                "generators.UnboundMethodCall.method_guard_chance", 0.5, 0, 1,
                "probability that the method load is wrapped in a try/catch");
REDLINE_TUNABLE(kUnboundMethodCallObjectReceiverChance,
                "generators.UnboundMethodCall.object_receiver_chance", 0.6, 0,
                1,
                "probability that the new receiver is an object-like value "
                "rather than any value");
REDLINE_TUNABLE(kUnboundMethodCallCallGuardChance,
                "generators.UnboundMethodCall.call_guard_chance", 0.8, 0, 1,
                "probability that the call/apply/bind invocation is wrapped in "
                "a try/catch");

// A method taken off its object and invoked with another `this`, or bound:
// `const m = obj.method; m.call(other, ...)`, `obj.method.bind(other)(...)`.
GEN(UnboundMethodCall) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Identifier* object = cg.Variable(JsType::kUnknown);
  if (object == nullptr) {
    return false;
  }
  JsType type = JsType::kUnknown;
  for (const Variable& v : ctx.visible) {
    if (v.name == object->name) {
      type = v.type;
      break;
    }
  }
  const BuiltinType* bt = ctx.env.ForType(type);
  std::string_view method_name;
  int min_args = 0, max_args = kUnboundMethodCallMaxArgs.AsInt();
  if (bt != nullptr && !bt->methods.empty() &&
      r.Chance(kUnboundMethodCallBuiltinChance())) {
    const BuiltinFunction& m = r.Pick(bt->methods);
    method_name = m.name;
    min_args = m.min_args;
    max_args = m.max_args;
  } else {
    method_name = r.Pick({"toString", "valueOf", "call", "apply", "bind",
                          "constructor", "hasOwnProperty", "toLocaleString"});
  }
  cg.DeclareMaybeGuarded(out, MemberOf(t, mk::Id(t, object->name), method_name),
                         JsType::kFunction, "m",
                         kUnboundMethodCallMethodGuardChance());
  std::string_view m = LastDeclared(ctx);
  Expression* receiver = r.Chance(kUnboundMethodCallObjectReceiverChance())
                             ? cg.ObjectLike()
                             : cg.Value(JsType::kUnknown, 1);
  std::vector<Expression*> args = cg.Arguments(min_args, max_args);
  Expression* call;
  switch (r.Index(3)) {
    case 0:
      args.insert(args.begin(), receiver);
      call = mk::CallMethod(t, mk::Id(t, m), "call", std::move(args));
      break;
    case 1:
      call = mk::CallMethod(t, mk::Id(t, m), "apply",
                            {receiver, mk::Array(t, std::move(args))});
      break;
    default:
      call = mk::Call(t, mk::CallMethod(t, mk::Id(t, m), "bind", {receiver}),
                      std::move(args));
      break;
  }
  cg.DeclareMaybeGuarded(out, call, JsType::kUnknown, "r",
                         kUnboundMethodCallCallGuardChance());
  return true;
}

REDLINE_TUNABLE(kBuiltinOverwritePrototypeChance,
                "generators.BuiltinOverwrite.prototype_chance", 0.6, 0, 1,
                "probability of overwriting a prototype method rather than a "
                "static method of a namespace");
REDLINE_TUNABLE(kBuiltinOverwriteSymbolChance,
                "generators.BuiltinOverwrite.symbol_chance", 0.25, 0, 1,
                "probability that the overwritten prototype member is a "
                "well-known symbol hook rather than a named method");
REDLINE_TUNABLE(kBuiltinOverwriteCallableChance,
                "generators.BuiltinOverwrite.callable_chance", 0.3, 0, 1,
                "probability that the replacement is an existing callable "
                "rather than a fresh function");

// Replaces a builtin (prototype) method or a well-known symbol hook by a
// function of our own: `Array.prototype.join = function(...) {...}`,
// `Object.prototype[Symbol.toPrimitive] = ...`, `Math.max = ...`.
GEN(BuiltinOverwrite) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* target = nullptr;
  if (r.Chance(kBuiltinOverwritePrototypeChance())) {
    struct Entry {
      const char* ctor;
      JsType type;
    };
    static const Entry kEntries[] = {
        {"Array", JsType::kArray},     {"String", JsType::kString},
        {"Object", JsType::kObject},   {"Function", JsType::kFunction},
        {"Number", JsType::kNumber},   {"RegExp", JsType::kRegExp},
        {"Promise", JsType::kPromise}, {"Map", JsType::kMap}};
    const Entry& e = kEntries[r.Index(sizeof(kEntries) / sizeof(kEntries[0]))];
    const BuiltinType* bt = ctx.env.ForType(e.type);
    if (bt == nullptr || bt->methods.empty()) {
      return false;
    }
    Expression* proto = mk::Member(t, mk::Id(t, e.ctor), "prototype");
    if (r.Chance(kBuiltinOverwriteSymbolChance())) {
      target = mk::Index(
          t, proto,
          mk::Member(t, mk::Id(t, "Symbol"),
                     r.Pick({"iterator", "toPrimitive", "hasInstance",
                             "species", "toStringTag", "isConcatSpreadable",
                             "match", "replace", "split"})));
    } else {
      target = mk::Member(t, proto, r.Pick(bt->methods).name);
    }
  } else {
    std::vector<const BuiltinGlobal*> namespaces;
    for (const BuiltinGlobal& g : ctx.env.globals()) {
      if (!g.static_methods.empty()) {
        namespaces.push_back(&g);
      }
    }
    if (namespaces.empty()) {
      return false;
    }
    const BuiltinGlobal& g = *r.Pick(namespaces);
    target = mk::Member(t, mk::Id(t, g.name), r.Pick(g.static_methods).name);
  }
  Expression* replacement = r.Chance(kBuiltinOverwriteCallableChance())
                                ? static_cast<Expression*>(cg.Callable())
                                : cg.FunctionValue(ctx.depth + 1);
  Push(out, cg.Stmt(mk::Assign(t, target, replacement)));
  return true;
}

REDLINE_TUNABLE(kConstructorFunctionMinParams,
                "generators.ConstructorFunction.min_params", 1, 1, 16,
                "fewest parameters of the constructor function (the first one "
                "is passed to the new.target fallback)");
REDLINE_TUNABLE(kConstructorFunctionMaxParams,
                "generators.ConstructorFunction.max_params", 3, 1, 16,
                "most parameters of the constructor function");
REDLINE_TUNABLE(kConstructorFunctionMaxFields,
                "generators.ConstructorFunction.max_fields", 4, 1, 64,
                "most this.x = ... initializations in the constructor body");
REDLINE_TUNABLE(kConstructorFunctionParamFieldChance,
                "generators.ConstructorFunction.param_field_chance", 0.6, 0, 1,
                "probability that a field is initialized from the matching "
                "parameter rather than a fresh value");
REDLINE_TUNABLE(
    kConstructorFunctionLengthChance,
    "generators.ConstructorFunction.length_chance", 0.3, 0, 1,
    "probability of an extra this.length = <integer> initialization");
REDLINE_TUNABLE(kConstructorFunctionPrototypeMethodChance,
                "generators.ConstructorFunction.prototype_method_chance", 0.3,
                0, 1, "probability of adding a method to C.prototype");

// A constructor function in the old style, with `new.target`, `this.x =`
// initialization, default and rest parameters, then instantiated.
GEN(ConstructorFunction) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view name = ctx.builder.FreshName("C");
  GenContext nested = ctx.Nested(true, false, false, false);
  std::vector<std::string_view> params;
  int np = static_cast<int>(r.Range(kConstructorFunctionMinParams.AsInt(),
                                    kConstructorFunctionMaxParams.AsInt()));
  for (int i = 0; i < np; i++) {
    std::string_view p = ctx.builder.FreshName("a");
    params.push_back(p);
    nested.Declare(p, JsType::kUnknown);
  }
  CodeGen inner(nested);
  Statements body;
  // if (!new.target) { throw ... } / return new C(...)
  body.push_back(
      mk::If(t, mk::Unary(t, UnaryOp::kNot, mk::NewTarget(t)),
             mk::Block(t, {mk::Return(t, mk::New(t, mk::Id(t, name),
                                                 {mk::Id(t, params[0])}))})));
  int fields = 1 + r.SmallCount(kConstructorFunctionMaxFields.AsInt() - 1);
  for (int i = 0; i < fields; i++) {
    Expression* value =
        i < np && r.Chance(kConstructorFunctionParamFieldChance())
            ? static_cast<Expression*>(
                  mk::Id(t, params[static_cast<size_t>(i)]))
            : inner.Value(JsType::kUnknown, 1);
    body.push_back(mk::ExprStmt(
        t, mk::Assign(t,
                      mk::Member(t, mk::This(t),
                                 ctx.env.property_names()[r.Index(12)]),
                      value)));
  }
  if (r.Chance(kConstructorFunctionLengthChance())) {
    body.push_back(mk::ExprStmt(
        t,
        mk::Assign(t, mk::Member(t, mk::This(t), "length"), inner.Integer())));
  }
  FunctionDeclaration* decl =
      mk::FunctionDecl(t, name, params, std::move(body));
  Push(out, decl);
  ctx.Declare(name, JsType::kClass);
  if (r.Chance(kConstructorFunctionPrototypeMethodChance())) {
    // A prototype method that uses this.
    GenContext method_ctx = ctx.Nested(true, false, false, false);
    CodeGen method_cg(method_ctx);
    Statements mbody;
    mbody.push_back(mk::Return(
        t, mk::Member(t, mk::This(t), ctx.env.property_names()[r.Index(12)])));
    Push(out, cg.Stmt(mk::Assign(
                  t,
                  mk::Member(t, mk::Member(t, mk::Id(t, name), "prototype"),
                             r.Pick({"get", "valueOf", "toString", "m", "f"})),
                  mk::Function(t, "", {}, std::move(mbody)))));
  }
  Push(out, cg.Declare(mk::New(t, mk::Id(t, name), cg.Arguments(0, np)),
                       JsType::kObject, "o"));
  return true;
}

// --- async / generators
// ---------------------------------------------------------

GEN(Await) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* value = nullptr;
  switch (r.Index(4)) {
    case 0:
      value = cg.Value(JsType::kPromise, 1);
      break;
    case 1:
      value = mk::CallMethod(t, mk::Id(t, "Promise"),
                             r.Coin() ? "resolve" : "reject",
                             {cg.Value(JsType::kUnknown, 1)});
      break;
    default:
      value = cg.Value(JsType::kUnknown, 1);
      break;
  }
  Expression* a = mk::Await(t, value);
  if (r.Coin()) {
    Push(out, cg.Declare(a, JsType::kUnknown));
  } else {
    Push(out, cg.Stmt(a));
  }
  return true;
}

REDLINE_TUNABLE(kYieldDelegateChance, "generators.Yield.delegate_chance", 0.2,
                0, 1, "probability of yield* over an array");
REDLINE_TUNABLE(kYieldValueChance, "generators.Yield.value_chance", 0.9, 0, 1,
                "probability that a plain yield has a value");

GEN(Yield) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* y = nullptr;
  if (r.Chance(kYieldDelegateChance())) {
    Expression* iterable = cg.Variable(JsType::kArray);
    if (iterable == nullptr) {
      iterable = cg.ArrayLiteral(1);
    }
    y = mk::Yield(t, iterable, /*delegate=*/true);
  } else {
    y = mk::Yield(t, r.Chance(kYieldValueChance())
                         ? cg.Value(JsType::kUnknown, 1)
                         : nullptr);
  }
  if (r.Coin()) {
    Push(out, cg.Declare(y, JsType::kUnknown));
  } else {
    Push(out, cg.Stmt(y));
  }
  return true;
}

REDLINE_TUNABLE(kAsyncIifeMaxStatements, "generators.AsyncIife.max_statements",
                4, 1, 64, "most statements in the async IIFE body");

GEN(AsyncIife) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  GenContext nested = ctx.Nested(true, true, true, false);
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, 1 + r.SmallCount(kAsyncIifeMaxStatements.AsInt() - 1));
  // Make sure there is an await in there.
  Statements await_stmt;
  GenAwait(nested, inner, &await_stmt);
  body.insert(
      body.begin() + static_cast<std::ptrdiff_t>(r.Index(body.size() + 1)),
      await_stmt.begin(), await_stmt.end());
  Expression* fn = mk::Arrow(t, {}, std::move(body), /*is_async=*/true);
  Expression* call = mk::Call(t, fn, {});
  if (r.Coin()) {
    Push(out, cg.Declare(call, JsType::kPromise, "p"));
  } else {
    Push(out, cg.Stmt(call));
  }
  return true;
}

REDLINE_TUNABLE(kGeneratorIterateMaxStatements,
                "generators.GeneratorIterate.max_statements", 2, 0, 64,
                "most statements in the generator body besides the yields");
REDLINE_TUNABLE(kGeneratorIterateMaxYields,
                "generators.GeneratorIterate.max_yields", 4, 1, 64,
                "most yields in the generator body");
REDLINE_TUNABLE(
    kGeneratorIterateLoopMaxStatements,
    "generators.GeneratorIterate.loop_max_statements", 3, 1, 64,
    "most statements in the for-of body that consumes the generator");
REDLINE_TUNABLE(kGeneratorIterateMaxSteps,
                "generators.GeneratorIterate.max_steps", 4, 1, 64,
                "most manual it.next()/it.return() steps when the generator is "
                "not consumed by a loop");
REDLINE_TUNABLE(
    kGeneratorIterateNextChance, "generators.GeneratorIterate.next_chance", 0.8,
    0, 1,
    "probability that a manual step is it.next() rather than it.return()");

GEN(GeneratorIterate) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  // function* g() { yield ...; } for (const x of g()) {...}
  std::string_view name = ctx.builder.FreshName("gen");
  GenContext nested = ctx.Nested(true, false, false, true);
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, r.SmallCount(kGeneratorIterateMaxStatements.AsInt()));
  int yields = 1 + r.SmallCount(kGeneratorIterateMaxYields.AsInt() - 1);
  for (int i = 0; i < yields; i++) {
    Statements y;
    GenYield(nested, inner, &y);
    body.insert(
        body.begin() + static_cast<std::ptrdiff_t>(r.Index(body.size() + 1)),
        y.begin(), y.end());
  }
  Push(out, mk::FunctionDecl(t, name, {}, std::move(body), false, true));
  ctx.Declare(name, JsType::kFunction);
  if (r.Coin()) {
    std::string_view e = ctx.builder.FreshName("e");
    GenContext loop = ctx.Inner(true, false);
    loop.Declare(e, JsType::kUnknown);
    CodeGen lcg(loop);
    Statements lbody = lcg.Body(
        loop, 1 + r.SmallCount(kGeneratorIterateLoopMaxStatements.AsInt() - 1));
    Push(out,
         mk::ForOf(t, VariableKind::kConst, e, mk::Call(t, mk::Id(t, name), {}),
                   mk::Block(t, std::move(lbody))));
  } else {
    Statement* it =
        cg.Declare(mk::Call(t, mk::Id(t, name), {}), JsType::kObject, "it");
    Push(out, it);
    std::string_view it_name = it->Cast<VariableDeclaration>()
                                   ->declarations[0]
                                   ->target->Cast<Identifier>()
                                   ->name;
    int steps = 1 + r.SmallCount(kGeneratorIterateMaxSteps.AsInt() - 1);
    for (int i = 0; i < steps; i++) {
      Push(out,
           cg.Declare(
               mk::CallMethod(
                   t, mk::Id(t, it_name),
                   r.Chance(kGeneratorIterateNextChance()) ? "next" : "return",
                   r.Coin() ? std::vector<Expression*>{}
                            : std::vector<Expression*>{cg.Value(
                                  JsType::kUnknown, 1)}),
               JsType::kObject));
    }
  }
  return true;
}

REDLINE_TUNABLE(kPromiseThenResolveChance,
                "generators.PromiseThen.resolve_chance", 0.8, 0, 1,
                "probability of Promise.resolve rather than Promise.reject "
                "when no promise variable is visible");
REDLINE_TUNABLE(
    kPromiseThenMaxStatements, "generators.PromiseThen.max_statements", 3, 1,
    64, "most statements in the fulfillment callback before its return");
REDLINE_TUNABLE(kPromiseThenThenChance, "generators.PromiseThen.then_chance",
                0.8, 0, 1, "probability of .then rather than .finally");

GEN(PromiseThen) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* promise = nullptr;
  if (Identifier* p = cg.Variable(JsType::kPromise)) {
    promise = p;
  } else {
    promise = mk::CallMethod(
        t, mk::Id(t, "Promise"),
        r.Chance(kPromiseThenResolveChance()) ? "resolve" : "reject",
        {cg.Value(JsType::kUnknown, 1)});
  }
  std::string_view p = ctx.builder.FreshName("v");
  GenContext nested = ctx.Nested(true, true, false, false);
  nested.Declare(p, JsType::kUnknown);
  CodeGen inner(nested);
  Statements body = inner.Body(
      nested, 1 + r.SmallCount(kPromiseThenMaxStatements.AsInt() - 1));
  body.push_back(mk::Return(t, inner.Value(JsType::kUnknown, 1)));
  std::vector<Expression*> args = {mk::Arrow(t, {p}, std::move(body))};
  if (r.Coin()) {
    std::string_view e = ctx.builder.FreshName("e");
    args.push_back(mk::ArrowExpr(t, {e}, mk::Id(t, e)));
  }
  Expression* call = mk::CallMethod(
      t, promise, r.Chance(kPromiseThenThenChance()) ? "then" : "finally",
      std::move(args));
  if (r.Coin()) {
    Push(out, cg.Declare(call, JsType::kPromise, "p"));
  } else {
    Push(out, cg.Stmt(call));
  }
  return true;
}

GEN(StringOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* s = cg.Value(JsType::kString, 1);
  Expression* v = nullptr;
  switch (r.Index(6)) {
    case 0:
      v = mk::Binary(t, BinaryOp::kAdd, s, cg.Value(JsType::kUnknown, 1));
      break;
    case 1:
      v = mk::CallMethod(t, s, "repeat", {cg.Integer()});
      break;
    case 2:
      v = mk::Index(t, s, cg.Integer());
      break;
    case 3:
      v = mk::Native(t, r.Pick({"InternalizeString", "FlattenString"}), {s});
      break;
    case 4:
      v = mk::CallMethod(t, s,
                         r.Pick({"charCodeAt", "codePointAt", "at", "slice",
                                 "indexOf", "split"}),
                         {cg.Value(JsType::kUnknown, 1)});
      break;
    default: {
      Expression* target_str =
          r.Coin() ? s : mk::Str(t, "a\u0301b\u0323c\u0304");
      v = mk::CallMethod(t, target_str, "normalize",
                         {mk::Str(t, r.Pick({"NFC", "NFD", "NFKC", "NFKD"}))});
      break;
    }
  }
  Push(out, cg.Declare(v, JsType::kUnknown, "s"));
  return true;
}

REDLINE_TUNABLE(kIntlOpsMaxCalls, "generators.IntlOps.max_calls", 3, 1, 16,
                "most method calls on the Intl object after its construction");
REDLINE_TUNABLE(kIntlOpsGuardChance, "generators.IntlOps.guard_chance", 0.5, 0,
                1,
                "probability that a call is wrapped in try/catch (values from "
                "the program can make it throw)");

// One Intl service: a constructor, consistent options and its methods. The
// locales and options are valid, so that ICU's locale and formatting code
// runs instead of the argument checks.
GEN(IntlOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  auto str = [&](std::string_view s) -> Expression* { return mk::Str(t, s); };
  auto locale = [&]() -> Expression* {
    static const char* const kLocales[] = {"en",
                                           "en-US",
                                           "de-DE",
                                           "fr-CA",
                                           "ja-JP",
                                           "zh-Hant-TW",
                                           "ar-EG",
                                           "he-IL",
                                           "th-TH-u-nu-thai",
                                           "hi-IN",
                                           "ru",
                                           "tr",
                                           "und",
                                           "en-u-ca-buddhist",
                                           "de-u-co-phonebk",
                                           "es-419",
                                           "sr-Latn-RS",
                                           "pt-BR",
                                           "ko-KR",
                                           "fa-IR-u-nu-arabext",
                                           "ja-JP-u-ca-japanese",
                                           "en-GB-u-hc-h23"};
    auto pick = [&]() {
      return str(kLocales[r.Index(sizeof(kLocales) / sizeof(kLocales[0]))]);
    };
    switch (r.Index(5)) {
      case 0:
        return mk::Undefined(t);
      case 1:
        return mk::Array(t, {pick(), pick()});
      default:
        return pick();
    }
  };
  auto option = [&](std::vector<Node*>* props, std::string_view key,
                    std::initializer_list<const char*> values) {
    props->push_back(
        mk::Prop(t, key, str(*(values.begin() + r.Index(values.size())))));
  };
  auto number = [&]() -> Expression* {
    return r.Coin() ? cg.Value(JsType::kNumber, 1) : cg.Number();
  };
  auto string = [&]() -> Expression* {
    return r.Coin() ? cg.Value(JsType::kString, 1) : cg.String();
  };
  auto date = [&]() -> Expression* {
    return mk::New(
        t, mk::Id(t, "Date"),
        {r.Coin()
             ? number()
             : mk::Num(t, 1.7e12 + static_cast<double>(r.Range(0, 1000000)))});
  };

  std::vector<Node*> props;
  std::string_view ctor;
  std::vector<std::pair<std::string_view, int>>
      methods;  // name, kind of argument
  enum {
    kNumberArg,
    kStringArg,
    kDateArg,
    kTwoNumbers,
    kTwoDates,
    kTwoStrings,
    kStringList,
    kNone,
    kUnit
  };
  switch (r.Index(10)) {
    case 0: {
      ctor = "NumberFormat";
      const char* style = r.Pick({"decimal", "percent", "currency", "unit"});
      props.push_back(mk::Prop(t, "style", str(style)));
      if (std::string_view(style) == "currency") {
        option(&props, "currency", {"EUR", "JPY", "USD", "BHD", "CHF"});
        option(&props, "currencyDisplay",
               {"symbol", "code", "name", "narrowSymbol"});
      }
      if (std::string_view(style) == "unit") {
        option(&props, "unit",
               {"kilometer-per-hour", "liter", "byte", "percent", "celsius"});
        option(&props, "unitDisplay", {"short", "long", "narrow"});
      }
      if (r.Coin()) {
        option(&props, "notation",
               {"standard", "scientific", "engineering", "compact"});
      }
      if (r.Coin()) {
        option(&props, "signDisplay",
               {"auto", "always", "exceptZero", "negative", "never"});
      }
      if (r.Coin()) {
        option(&props, "roundingMode",
               {"ceil", "floor", "halfEven", "halfExpand", "trunc"});
        props.push_back(mk::Prop(t, "maximumFractionDigits",
                                 mk::Num(t, static_cast<double>(r.Index(6)))));
      }
      methods = {{"format", kNumberArg},
                 {"formatToParts", kNumberArg},
                 {"formatRange", kTwoNumbers},
                 {"resolvedOptions", kNone}};
      break;
    }
    case 1: {
      ctor = "DateTimeFormat";
      if (r.Coin()) {
        option(&props, "dateStyle", {"full", "long", "medium", "short"});
        option(&props, "timeStyle", {"full", "long", "medium", "short"});
      } else {
        option(&props, "weekday", {"long", "short", "narrow"});
        option(&props, "hour", {"numeric", "2-digit"});
        option(&props, "era", {"long", "short", "narrow"});
        if (r.Coin()) {
          props.push_back(
              mk::Prop(t, "fractionalSecondDigits",
                       mk::Num(t, static_cast<double>(1 + r.Index(3)))));
        }
      }
      option(&props, "timeZone",
             {"UTC", "Asia/Kolkata", "America/New_York", "Europe/Berlin",
              "Pacific/Chatham"});
      if (r.Coin()) {
        option(&props, "calendar",
               {"gregory", "islamic", "hebrew", "japanese", "chinese",
                "ethiopic"});
      }
      methods = {{"format", kDateArg},
                 {"formatToParts", kDateArg},
                 {"formatRange", kTwoDates},
                 {"formatRangeToParts", kTwoDates},
                 {"resolvedOptions", kNone}};
      break;
    }
    case 2:
      ctor = "Collator";
      option(&props, "sensitivity", {"base", "accent", "case", "variant"});
      if (r.Coin()) {
        props.push_back(mk::Prop(t, "numeric", mk::Bool(t, r.Coin())));
        option(&props, "caseFirst", {"upper", "lower", "false"});
      }
      option(&props, "usage", {"sort", "search"});
      methods = {{"compare", kTwoStrings}, {"resolvedOptions", kNone}};
      break;
    case 3:
      ctor = "PluralRules";
      option(&props, "type", {"cardinal", "ordinal"});
      methods = {{"select", kNumberArg},
                 {"selectRange", kTwoNumbers},
                 {"resolvedOptions", kNone}};
      break;
    case 4:
      ctor = "RelativeTimeFormat";
      option(&props, "numeric", {"always", "auto"});
      option(&props, "style", {"long", "short", "narrow"});
      methods = {{"format", kUnit},
                 {"formatToParts", kUnit},
                 {"resolvedOptions", kNone}};
      break;
    case 5:
      ctor = "ListFormat";
      option(&props, "type", {"conjunction", "disjunction", "unit"});
      option(&props, "style", {"long", "short", "narrow"});
      methods = {{"format", kStringList},
                 {"formatToParts", kStringList},
                 {"resolvedOptions", kNone}};
      break;
    case 6:
      ctor = "DisplayNames";
      option(&props, "type",
             {"region", "language", "script", "currency", "calendar",
              "dateTimeField"});
      option(&props, "fallback", {"code", "none"});
      methods = {{"of", kStringArg}, {"resolvedOptions", kNone}};
      break;
    case 7:
      ctor = "Segmenter";
      option(&props, "granularity", {"grapheme", "word", "sentence"});
      methods = {{"segment", kStringArg}, {"resolvedOptions", kNone}};
      break;
    case 8:
      ctor = "DurationFormat";
      option(&props, "style", {"long", "short", "narrow", "digital"});
      methods = {{"format", kNone},
                 {"formatToParts", kNone},
                 {"resolvedOptions", kNone}};
      break;
    default: {
      // Primitive methods that take locales and options.
      Expression* v = nullptr;
      switch (r.Index(4)) {
        case 0:
          v = mk::CallMethod(t, number(), "toLocaleString", {locale()});
          break;
        case 1:
          v = mk::CallMethod(
              t, date(), r.Pick({"toLocaleDateString", "toLocaleTimeString"}),
              {locale()});
          break;
        case 2:
          v = mk::CallMethod(t, string(), "localeCompare",
                             {string(), locale()});
          break;
        default:
          v = mk::CallMethod(t, string(),
                             r.Pick({"toLocaleUpperCase", "toLocaleLowerCase"}),
                             {locale()});
          break;
      }
      cg.DeclareMaybeGuarded(out, v, JsType::kUnknown, "loc",
                             kIntlOpsGuardChance());
      return true;
    }
  }
  // const f = new Intl.X(locales, {...});
  Expression* construct = mk::New(t, mk::Member(t, mk::Id(t, "Intl"), ctor),
                                  {locale(), mk::Object(t, std::move(props))});
  Statement* decl = cg.Declare(construct, JsType::kObject, "intl");
  Push(out, decl);
  std::string_view name = decl->Cast<VariableDeclaration>()
                              ->declarations[0]
                              ->target->Cast<Identifier>()
                              ->name;
  int calls = 1 + static_cast<int>(
                      r.Index(static_cast<size_t>(kIntlOpsMaxCalls.AsInt())));
  for (int i = 0; i < calls; i++) {
    const auto& method = methods[r.Index(methods.size())];
    std::vector<Expression*> args;
    switch (method.second) {
      case kNumberArg:
        args = {r.Chance(0.2) ? cg.BigInt() : number()};
        break;
      case kStringArg:
        args = {r.Chance(0.5)
                    ? str(r.Pick({"US", "de", "Latn", "EUR", "gregory", "era",
                                  "a\u0301b\U0001F600c d."}))
                    : string()};
        break;
      case kDateArg:
        args = {date()};
        break;
      case kTwoNumbers:
        args = {number(), number()};
        break;
      case kTwoDates:
        args = {date(), date()};
        break;
      case kTwoStrings:
        args = {string(), string()};
        break;
      case kStringList:
        args = {mk::Array(t, {string(), string(), str("x")})};
        break;
      case kUnit:
        args = {number(),
                str(r.Pick({"second", "minutes", "day", "quarter", "years"}))};
        break;
      default:
        if (ctor == "DurationFormat" && method.first != "resolvedOptions") {
          args = {mk::Object(
              t, {mk::Prop(t, "hours",
                           mk::Num(t, static_cast<double>(r.Index(30)))),
                  mk::Prop(t, "minutes", number())})};
        }
        break;
    }
    Expression* call =
        mk::CallMethod(t, mk::Id(t, name), method.first, std::move(args));
    if (method.first == "segment" && r.Coin()) {
      call = mk::Array(t, {mk::Spread(t, call)});  // iterate the segments
    }
    cg.DeclareMaybeGuarded(out, call, JsType::kUnknown, "r",
                           kIntlOpsGuardChance());
  }
  return true;
}

REDLINE_TUNABLE(kTypedArrayOpsMaxArgs, "generators.TypedArrayOps.max_args", 2,
                0, 16, "most arguments passed to a typed array method");

GEN(TypedArrayOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Identifier* ta = cg.Variable(JsType::kTypedArray);
  if (ta == nullptr) {
    Expression* ctor = cg.Construct(JsType::kTypedArray);
    if (ctor == nullptr) {
      return false;
    }
    Statement* decl = cg.Declare(ctor, JsType::kTypedArray, "ta");
    Push(out, decl);
    ta = mk::Id(t, decl->Cast<VariableDeclaration>()
                       ->declarations[0]
                       ->target->Cast<Identifier>()
                       ->name);
  }
  Identifier* a = mk::Id(t, ta->name);

  std::vector<const char*> all_kinds = {
      "Uint8Array",   "Uint8ClampedArray", "Int8Array",     "Uint16Array",
      "Int16Array",   "Uint32Array",       "Int32Array",    "Float32Array",
      "Float64Array", "BigInt64Array",     "BigUint64Array"};
  if (cg.env().Global("Float16Array") != nullptr) {
    all_kinds.push_back("Float16Array");
  }
  const char* random_kind = r.Pick(all_kinds);

  switch (r.Index(8)) {
    case 0:
      Push(out, cg.Stmt(mk::Assign(t, mk::Index(t, a, cg.Integer()),
                                   cg.Value(JsType::kNumber, 1))));
      break;
    case 1:
      Push(out, cg.Declare(mk::Index(t, a, cg.Integer()), JsType::kNumber));
      break;
    case 2:
      Push(out, cg.Stmt(mk::Native(t, "ArrayBufferDetach",
                                   {mk::Member(t, a, "buffer")})));
      break;
    case 3:
      Push(out, cg.Declare(mk::CallMethod(
                               t, a, r.Pick({"fill", "map", "sort"}),
                               cg.Arguments(0, kTypedArrayOpsMaxArgs.AsInt())),
                           JsType::kUnknown));
      break;
    case 4:
      // original new from buffer, modified to use all kinds
      Push(out, cg.Declare(mk::New(t, mk::Id(t, random_kind),
                                   {mk::Member(t, a, "buffer")}),
                           JsType::kTypedArray, "ta"));
      break;
    case 5: {
      // set with another typed array or plain array
      Expression* source = r.Coin() ? cg.Value(JsType::kArray, 1)
                                    : cg.Value(JsType::kTypedArray, 1);
      Push(out, cg.Stmt(mk::CallMethod(t, a, "set", {source})));
      break;
    }
    case 6: {
      // constructors from another typed array, from, of, subarray, slice across
      // element kinds
      const char* op =
          r.Pick({"from", "of", "subarray", "slice", "constructor"});
      if (std::string_view(op) == "constructor") {
        Push(out, cg.Declare(mk::New(t, mk::Id(t, random_kind), {a}),
                             JsType::kTypedArray, "ta"));
      } else if (std::string_view(op) == "from" ||
                 std::string_view(op) == "of") {
        Expression* arg = (std::string_view(op) == "from")
                              ? static_cast<Expression*>(a)
                              : static_cast<Expression*>(mk::Spread(t, a));
        Push(out,
             cg.Declare(mk::CallMethod(t, mk::Id(t, random_kind), op, {arg}),
                        JsType::kTypedArray, "ta"));
      } else {  // subarray, slice
        Push(out,
             cg.Declare(mk::CallMethod(t, a, op, {cg.Integer(), cg.Integer()}),
                        JsType::kUnknown));
      }
      break;
    }
    default:
      Push(out, cg.Declare(mk::Member(t, a,
                                      r.Pick({"length", "byteLength",
                                              "byteOffset", "buffer"})),
                           JsType::kUnknown));
      break;
  }
  return true;
}

REDLINE_TUNABLE(kFrozenArrayOpsMaxArgs, "generators.FrozenArrayOps.max_args", 3,
                0, 16, "most arguments passed to a frozen array method");

GEN(FrozenArrayOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* arr = cg.ArrayLiteral(1);
  Expression* sealed_or_frozen = mk::CallMethod(
      t, mk::Id(t, "Object"), r.Coin() ? "freeze" : "seal", {arr});
  Statement* decl = cg.Declare(sealed_or_frozen, JsType::kArray, "farr");
  Push(out, decl);
  Identifier* a = mk::Id(t, decl->Cast<VariableDeclaration>()
                                ->declarations[0]
                                ->target->Cast<Identifier>()
                                ->name);

  switch (r.Index(3)) {
    case 0:
      Push(out, cg.Declare(mk::CallMethod(t, a, r.Pick({"includes", "indexOf"}),
                                          cg.Arguments(1, 2)),
                           JsType::kUnknown));
      break;
    case 1:
      Push(out, cg.Declare(mk::CallMethod(
                               t, a, "concat",
                               cg.Arguments(1, kFrozenArrayOpsMaxArgs.AsInt())),
                           JsType::kUnknown));
      break;
    default: {
      Expression* obj = mk::Object(
          t,
          {mk::Prop(t, mk::Member(t, mk::Id(t, "Symbol"), "isConcatSpreadable"),
                    mk::Bool(t, r.Coin()), true),
           mk::Prop(t, "length", cg.Integer()),
           mk::Prop(t, "0", cg.Value(JsType::kUnknown, 1))});
      Push(out,
           cg.Declare(mk::CallMethod(t, a, "concat", {obj}), JsType::kUnknown));
      break;
    }
  }
  return true;
}

REDLINE_TUNABLE(
    kTemporalOpsMaxCalls, "generators.TemporalOps.max_calls", 3, 1, 16,
    "most method calls on the Temporal object after its construction");
REDLINE_TUNABLE(kTemporalOpsGuardChance, "generators.TemporalOps.guard_chance",
                0.9, 0, 1,
                "probability that a call is wrapped in try/catch (values from "
                "the program can make it throw)");

GEN(TemporalOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);

  auto create_temporal = [&](const char* ctor) -> Expression* {
    std::vector<Expression*> args;
    if (std::string_view(ctor) == "PlainDate") {
      args = {mk::Num(t, 2023), mk::Num(t, static_cast<double>(r.Range(1, 13))),
              mk::Num(t, static_cast<double>(r.Range(1, 29)))};
    } else if (std::string_view(ctor) == "PlainTime") {
      args = {mk::Num(t, static_cast<double>(r.Range(0, 24))),
              mk::Num(t, static_cast<double>(r.Range(0, 60))),
              mk::Num(t, static_cast<double>(r.Range(0, 60)))};
    } else if (std::string_view(ctor) == "PlainDateTime") {
      args = {mk::Num(t, 2023), mk::Num(t, static_cast<double>(r.Range(1, 13))),
              mk::Num(t, static_cast<double>(r.Range(1, 29))),
              mk::Num(t, static_cast<double>(r.Range(0, 24)))};
    } else if (std::string_view(ctor) == "ZonedDateTime") {
      args = {mk::BigInt(t, "123456789"), mk::Str(t, "UTC")};
    } else if (std::string_view(ctor) == "Instant") {
      args = {mk::BigInt(t, "123456789")};
    } else if (std::string_view(ctor) == "Duration") {
      args = {mk::Num(t, static_cast<double>(r.Range(0, 11))),
              mk::Num(t, static_cast<double>(r.Range(0, 11)))};
    }
    return mk::New(t, mk::Member(t, mk::Id(t, "Temporal"), ctor),
                   std::move(args));
  };

  const char* ctors[] = {"PlainDate",     "PlainTime", "PlainDateTime",
                         "ZonedDateTime", "Instant",   "Duration"};
  const char* ctor = ctors[r.Index(sizeof(ctors) / sizeof(ctors[0]))];

  Expression* construct = create_temporal(ctor);
  Statement* decl = cg.Declare(construct, JsType::kObject, "temp");
  Push(out, decl);
  Identifier* a = mk::Id(t, decl->Cast<VariableDeclaration>()
                                ->declarations[0]
                                ->target->Cast<Identifier>()
                                ->name);

  int calls = 1 + static_cast<int>(r.Index(
                      static_cast<size_t>(kTemporalOpsMaxCalls.AsInt())));
  for (int i = 0; i < calls; i++) {
    std::vector<const char*> all_methods = {"add",   "subtract", "with",
                                            "round", "compare",  "toString"};
    if (std::string_view(ctor) != "Duration") {
      all_methods.push_back("since");
      all_methods.push_back("until");
    }
    const char* method = r.Pick(all_methods);
    std::vector<Expression*> args;

    if (std::string_view(method) == "compare") {
      args = {a, create_temporal(ctor)};
      Expression* call =
          mk::CallMethod(t, mk::Member(t, mk::Id(t, "Temporal"), ctor), method,
                         std::move(args));
      cg.DeclareMaybeGuarded(out, call, JsType::kNumber, "r",
                             kTemporalOpsGuardChance());
    } else if (std::string_view(method) == "toString") {
      if (r.Coin()) {
        args = {mk::Object(
            t, {mk::Prop(t, "fractionalSecondDigits", mk::Str(t, "auto"))})};
      }
      Expression* call = mk::CallMethod(t, a, method, std::move(args));
      cg.DeclareMaybeGuarded(out, call, JsType::kString, "r",
                             kTemporalOpsGuardChance());
    } else if (std::string_view(method) == "with") {
      if (std::string_view(ctor) == "PlainDate" ||
          std::string_view(ctor) == "PlainDateTime" ||
          std::string_view(ctor) == "ZonedDateTime") {
        args = {mk::Object(t, {mk::Prop(t, "year", mk::Num(t, 2024))})};
      } else if (std::string_view(ctor) == "PlainTime") {
        args = {mk::Object(t, {mk::Prop(t, "hour", mk::Num(t, 12))})};
      } else if (std::string_view(ctor) == "Duration") {
        args = {mk::Object(t, {mk::Prop(t, "hours", mk::Num(t, 5))})};
      } else {
        args = {a};
      }
      Expression* call = mk::CallMethod(t, a, method, std::move(args));
      cg.DeclareMaybeGuarded(out, call, JsType::kUnknown, "r",
                             kTemporalOpsGuardChance());
    } else if (std::string_view(method) == "add" ||
               std::string_view(method) == "subtract") {
      args = {create_temporal("Duration")};
      Expression* call = mk::CallMethod(t, a, method, std::move(args));
      cg.DeclareMaybeGuarded(out, call, JsType::kUnknown, "r",
                             kTemporalOpsGuardChance());
    } else if (std::string_view(method) == "since" ||
               std::string_view(method) == "until") {
      args = {create_temporal(ctor)};
      Expression* call = mk::CallMethod(t, a, method, std::move(args));
      cg.DeclareMaybeGuarded(out, call, JsType::kUnknown, "r",
                             kTemporalOpsGuardChance());
    } else {
      // round
      if (std::string_view(ctor) == "PlainDate") {
        args = {
            mk::Object(t, {mk::Prop(t, "smallestUnit", mk::Str(t, "day"))})};
      } else {
        args = {
            mk::Object(t, {mk::Prop(t, "smallestUnit", mk::Str(t, "second"))})};
      }
      Expression* call = mk::CallMethod(t, a, method, std::move(args));
      cg.DeclareMaybeGuarded(out, call, JsType::kUnknown, "r",
                             kTemporalOpsGuardChance());
    }
  }
  return true;
}

REDLINE_TUNABLE(kMapSetOpsMaxOps, "generators.MapSetOps.max_ops", 4, 1, 64,
                "most set/get/has/delete operations on the map or set");

GEN(MapSetOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  bool map = r.Coin();
  Identifier* m = cg.Variable(map ? JsType::kMap : JsType::kSet);
  if (m == nullptr) {
    Statement* decl =
        cg.Declare(mk::New(t, mk::Id(t, map ? "Map" : "Set"), {}),
                   map ? JsType::kMap : JsType::kSet, map ? "map" : "set");
    Push(out, decl);
    m = mk::Id(t, decl->Cast<VariableDeclaration>()
                      ->declarations[0]
                      ->target->Cast<Identifier>()
                      ->name);
  }
  int n = 1 + r.SmallCount(kMapSetOpsMaxOps.AsInt() - 1);
  for (int i = 0; i < n; i++) {
    Identifier* o = mk::Id(t, m->name);
    if (map) {
      switch (r.Index(4)) {
        case 0:
          Push(out, cg.Stmt(mk::CallMethod(t, o, "set",
                                           {cg.Value(JsType::kUnknown, 1),
                                            cg.Value(JsType::kUnknown, 1)})));
          break;
        case 1:
          Push(out, cg.Declare(mk::CallMethod(t, o, "get",
                                              {cg.Value(JsType::kUnknown, 1)}),
                               JsType::kUnknown));
          break;
        case 2:
          Push(out, cg.Stmt(mk::CallMethod(t, o, "delete",
                                           {cg.Value(JsType::kUnknown, 1)})));
          break;
        default:
          Push(out, cg.Declare(mk::CallMethod(t, o, "has",
                                              {cg.Value(JsType::kUnknown, 1)}),
                               JsType::kBoolean, "b"));
          break;
      }
    } else {
      switch (r.Index(3)) {
        case 0:
          Push(out, cg.Stmt(mk::CallMethod(t, o, "add",
                                           {cg.Value(JsType::kUnknown, 1)})));
          break;
        case 1:
          Push(out, cg.Stmt(mk::CallMethod(t, o, "delete",
                                           {cg.Value(JsType::kUnknown, 1)})));
          break;
        default:
          Push(out, cg.Declare(mk::CallMethod(t, o, "has",
                                              {cg.Value(JsType::kUnknown, 1)}),
                               JsType::kBoolean, "b"));
          break;
      }
    }
  }
  return true;
}

GEN(ArgumentsUse) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  Expression* v = nullptr;
  switch (r.Index(3)) {
    case 0:
      v = mk::Member(t, mk::Id(t, "arguments"), "length");
      break;
    case 1:
      v = mk::Index(t, mk::Id(t, "arguments"), cg.Integer());
      break;
    default:
      v = mk::Array(t, {mk::Spread(t, mk::Id(t, "arguments"))});
      break;
  }
  Push(out, cg.Declare(v, JsType::kUnknown));
  return true;
}

GEN(ThisUse) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  if (r.Coin()) {
    Push(out, cg.Declare(cg.Property(mk::This(t)), JsType::kUnknown));
  } else {
    std::string_view name = cg.PropertyName();
    if (name == "__proto__") {
      name = "x";
    }
    Push(out, cg.Stmt(mk::Assign(t, cg.Property(mk::This(t), name),
                                 cg.Value(JsType::kUnknown, 1))));
  }
  return true;
}

REDLINE_TUNABLE(
    kEvalExplicitStrictChance, "generators.Eval.explicit_strict_chance", 0.3, 0,
    1, "chance to explicitly prepend 'use strict' to the eval snippet");

GEN(Eval) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  // Indirect and direct eval of small snippets.
  std::vector<const char*> snippets = {"1+1",  "this",         "typeof x", "[]",
                                       "({})", "new Array(3)", "undefined"};
  if (!ctx.syntax.strict) {
    snippets.push_back("var a=1; a;");
    snippets.push_back("function f(){return 2;} f();");
  }
  const char* snippet = r.Pick(snippets);
  Expression* callee = mk::Id(t, "eval");
  if (r.Coin()) {
    callee = mk::Seq(t, {mk::Num(t, 0), callee});  // indirect
  }
  Expression* arg = mk::Str(t, snippet);
  bool use_strict = r.Chance(kEvalExplicitStrictChance());
  // if we added sloppy mode snippets, make sure we don't accidentally
  // strict-ify them
  if (std::string_view(snippet) == "var a=1; a;" ||
      std::string_view(snippet) == "function f(){return 2;} f();") {
    use_strict = false;
  }
  if (use_strict) {
    arg = mk::Binary(t, BinaryOp::kAdd, mk::Str(t, "'use strict'; "), arg);
  }
  Push(out, cg.Declare(mk::Call(t, callee, {arg}), JsType::kUnknown));
  return true;
}

REDLINE_TUNABLE(kClosureMaxStatements, "generators.Closure.max_statements", 2,
                0, 64,
                "most statements in the arrow body before the update of the "
                "captured variable");
REDLINE_TUNABLE(kClosureMaxCalls, "generators.Closure.max_calls", 4, 1, 64,
                "most calls of the closure after it is declared");

GEN(Closure) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  // let v = 0; const f = () => v++; f(); f();
  Statement* decl = cg.Declare(cg.Value(JsType::kNumber, 1), JsType::kNumber);
  Push(out, decl);
  std::string_view captured = decl->Cast<VariableDeclaration>()
                                  ->declarations[0]
                                  ->target->Cast<Identifier>()
                                  ->name;
  decl->Cast<VariableDeclaration>()->declaration_kind = VariableKind::kLet;
  for (Variable& v : ctx.visible) {
    if (v.name == captured) {
      v.is_constant = false;
    }
  }
  GenContext nested = ctx.Nested(true, true, false, false);
  CodeGen inner(nested);
  Statements body =
      inner.Body(nested, r.SmallCount(kClosureMaxStatements.AsInt()));
  body.push_back(inner.Stmt(mk::Assign(t, mk::Id(t, captured),
                                       inner.Value(JsType::kNumber, 1),
                                       CodeGen::RandomCompoundAssignOp(r))));
  body.push_back(mk::Return(t, mk::Id(t, captured)));
  Statement* fdecl =
      cg.Declare(mk::Arrow(t, {}, std::move(body)), JsType::kFunction, "f");
  Push(out, fdecl);
  std::string_view f = fdecl->Cast<VariableDeclaration>()
                           ->declarations[0]
                           ->target->Cast<Identifier>()
                           ->name;
  int calls = 1 + r.SmallCount(kClosureMaxCalls.AsInt() - 1);
  for (int i = 0; i < calls; i++) {
    Push(out, cg.Stmt(mk::Call(t, mk::Id(t, f), {})));
  }
  return true;
}

// --- stress patterns
// ------------------------------------------------------------------
//
// Each generator below builds a situation that V8 regressions keep coming
// back to; all are parameterized over the visible values and random counts,
// none reproduces a particular test.

REDLINE_TUNABLE(kRecursionUnboundedChance,
                "generators.Recursion.unbounded_chance", 0.5, 0, 1,
                "probability that the recursion has no depth guard and ends in "
                "a caught stack overflow");
REDLINE_TUNABLE(kRecursionMaxDepth, "generators.Recursion.max_depth", 5000, 1,
                100000, "largest depth guard of a bounded recursion");
REDLINE_TUNABLE(kRecursionConstructorChance,
                "generators.Recursion.constructor_chance", 0.4, 0, 1,
                "probability that the recursive function is a constructor "
                "recursing through new this.constructor()");
REDLINE_TUNABLE(
    kRecursionMaxStatements, "generators.Recursion.max_statements", 2, 0, 64,
    "most generated statements executed at every level of the recursion");

// A function that calls itself under try/catch, until a depth guard or until
// the stack overflows (the RangeError is then caught at every level on the
// way out). Whatever the body does at each level is interrupted at some
// depth and resumed while unwinding: literals, property definitions and map
// transitions left half-done there are a classic source of stale state.
GEN(Recursion) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view name = ctx.builder.FreshName("rec");
  std::string_view depth = ctx.builder.FreshName("d");
  bool unbounded = r.Chance(kRecursionUnboundedChance());
  bool as_ctor = r.Chance(kRecursionConstructorChance());
  GenContext nested = ctx.Nested(true, false, false, false);
  nested.Declare(depth, JsType::kNumber);
  CodeGen inner(nested);
  Statements body;
  if (as_ctor) {
    // if (!new.target) return new rec(d);
    body.push_back(
        mk::If(t, mk::Unary(t, UnaryOp::kNot, mk::NewTarget(t)),
               mk::Block(t, {mk::Return(t, mk::New(t, mk::Id(t, name),
                                                   {mk::Id(t, depth)}))})));
  }
  Statements level =
      inner.Body(nested, r.SmallCount(kRecursionMaxStatements.AsInt()));
  body.insert(body.end(), level.begin(), level.end());
  // The recursive call: through the name, or through this.constructor for
  // constructors (which also exercises the prototype chain lookup).
  Expression* callee =
      as_ctor && r.Coin()
          ? static_cast<Expression*>(mk::Member(t, mk::This(t), "constructor"))
          : static_cast<Expression*>(mk::Id(t, name));
  Expression* next =
      mk::Binary(t, BinaryOp::kAdd, mk::Id(t, depth), mk::Num(t, 1));
  Expression* call =
      as_ctor ? static_cast<Expression*>(mk::New(t, callee, {next}))
              : static_cast<Expression*>(mk::Call(t, callee, {next}));
  Statement* guarded =
      mk::Try(t, {mk::ExprStmt(t, call)}, ctx.builder.FreshName("e"), {});
  if (unbounded) {
    body.push_back(guarded);
  } else {
    double limit = static_cast<double>(r.Range(1, kRecursionMaxDepth.AsInt()));
    body.push_back(mk::If(
        t,
        mk::Binary(t, BinaryOp::kLessThan, mk::Id(t, depth), mk::Num(t, limit)),
        guarded));
  }
  // Statements the body may contain can run after the recursion returns
  // too: a second generated statement after the call keeps the unwinding
  // path busy.
  if (r.Coin()) {
    Statements after = inner.Body(nested, 1);
    body.insert(body.end(), after.begin(), after.end());
  }
  Push(out, mk::FunctionDecl(t, name, {depth}, std::move(body)));
  ctx.Declare(name, as_ctor ? JsType::kClass : JsType::kFunction);
  // Start it. The top-level call is guarded too: the overflow may happen
  // before the first level installed its handler.
  Expression* start = as_ctor ? static_cast<Expression*>(mk::New(
                                    t, mk::Id(t, name), {mk::Num(t, 0)}))
                              : static_cast<Expression*>(mk::Call(
                                    t, mk::Id(t, name), {mk::Num(t, 0)}));
  Push(out,
       mk::Try(t, {mk::ExprStmt(t, start)}, ctx.builder.FreshName("e"), {}));
  return true;
}

REDLINE_TUNABLE(kWrapperChainMaxLength, "generators.WrapperChain.max_length",
                50000, 1, 1000000,
                "longest chain built (the length is drawn from a heavy-tailed "
                "list capped here)");
REDLINE_TUNABLE(kWrapperChainOptimizeChance,
                "generators.WrapperChain.optimize_chance", 0.5, 0, 1,
                "probability that the consumer of the chain is a function that "
                "gets JIT-optimized");

// A value wrapped around itself N times in a loop (a bound function bound
// again, an object whose prototype is the previous one, an array or object
// nested in a new one, a proxy around a proxy, a string appended to), then
// consumed by an operation that walks the whole chain. Anything that recurses
// over such a chain (instanceof, call reduction in the optimizing compilers,
// prototype walks, JSON, string flattening) meets its depth limits here.
GEN(WrapperChain) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view w = ctx.builder.FreshName("w");
  std::string_view i = ctx.builder.FreshName("i");
  enum Kind { kBind, kProto, kArray, kObject, kProxy, kString };
  Kind kind = static_cast<Kind>(r.Index(6));
  Expression* seed = nullptr;
  Expression* wrap = nullptr;
  switch (kind) {
    case kBind:
      seed = cg.Callable();
      wrap = mk::CallMethod(
          t, mk::Id(t, w), "bind",
          r.Coin() ? std::vector<Expression*>{}
                   : std::vector<Expression*>{cg.Value(JsType::kUnknown, 1)});
      break;
    case kProto:
      seed = cg.ObjectLike();
      wrap = mk::CallMethod(t, mk::Id(t, "Object"), "create", {mk::Id(t, w)});
      break;
    case kArray:
      seed = cg.ArrayLiteral(1);
      wrap = mk::Array(t, {mk::Id(t, w)});
      break;
    case kObject: {
      std::string_view key = cg.PropertyName();
      seed = cg.ObjectLiteral(1);
      wrap = mk::Object(
          t, {IsIdentifierName(key) ? mk::Prop(t, key, mk::Id(t, w))
                                    : mk::Prop(t, mk::Str(t, key), mk::Id(t, w),
                                               /*computed=*/true)});
      break;
    }
    case kProxy:
      seed = cg.ObjectLike();
      wrap = mk::New(t, mk::Id(t, "Proxy"), {mk::Id(t, w), mk::Object(t, {})});
      break;
    case kString:
      seed = cg.String();
      wrap = mk::Binary(t, BinaryOp::kAdd, mk::Id(t, w), cg.String());
      break;
  }
  static const double kLengths[] = {10,   100,   1000,  1000,
                                    5000, 10000, 20000, 50000};
  double length =
      std::min(kLengths[r.Index(sizeof(kLengths) / sizeof(kLengths[0]))],
               static_cast<double>(kWrapperChainMaxLength.AsInt()));
  Push(out, mk::Let(t, w, seed));
  ctx.Declare(w, kind == kBind     ? JsType::kFunction
                 : kind == kString ? JsType::kString
                                   : JsType::kObject);
  Push(out,
       mk::CountedLoop(
           t, i, mk::Num(t, length),
           mk::Block(t, {mk::ExprStmt(t, mk::Assign(t, mk::Id(t, w), wrap))})));
  // A consumer that has to walk the chain.
  Expression* use = nullptr;
  switch (kind) {
    case kBind:
      switch (r.Index(3)) {
        case 0:
          use = mk::Binary(t, BinaryOp::kInstanceof,
                           cg.Value(JsType::kObject, 1), mk::Id(t, w));
          break;
        case 1:
          use = mk::Call(t, mk::Id(t, w), {});
          break;
        default:
          use = mk::New(t, mk::Id(t, w), {});
          break;
      }
      break;
    case kProto:
    case kProxy:
      switch (r.Index(3)) {
        case 0:
          use = cg.Property(mk::Id(t, w));
          break;
        case 1:
          use = mk::Binary(t, BinaryOp::kIn, mk::Str(t, cg.PropertyName()),
                           mk::Id(t, w));
          break;
        default:
          use =
              mk::CallMethod(t, mk::Id(t, "JSON"), "stringify", {mk::Id(t, w)});
          break;
      }
      break;
    case kArray:
    case kObject:
      use = r.Coin() ? static_cast<Expression*>(mk::CallMethod(
                           t, mk::Id(t, "JSON"), "stringify", {mk::Id(t, w)}))
                     : static_cast<Expression*>(
                           mk::Call(t, mk::Id(t, "String"), {mk::Id(t, w)}));
      break;
    case kString:
      use =
          r.Coin()
              ? static_cast<Expression*>(
                    mk::CallMethod(t, mk::Id(t, w), "indexOf", {cg.String()}))
              : static_cast<Expression*>(mk::Member(t, mk::Id(t, w), "length"));
      break;
  }
  Statement* guarded_use =
      mk::Try(t, {mk::ExprStmt(t, use)}, ctx.builder.FreshName("e"), {});
  if (!r.Chance(kWrapperChainOptimizeChance())) {
    Push(out, guarded_use);
    return true;
  }
  // The consumer inside a function the optimizing compiler gets to see
  // (the chain is a constant to it).
  std::string_view f = ctx.builder.FreshName("f");
  Push(out, mk::FunctionDecl(t, f, {}, {guarded_use}));
  ctx.Declare(f, JsType::kFunction);
  Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                               {mk::Id(t, f)})));
  Push(out, cg.Stmt(mk::Call(t, mk::Id(t, f), {})));
  Push(out,
       cg.Stmt(mk::Native(t, "OptimizeFunctionOnNextCall", {mk::Id(t, f)})));
  Push(out, cg.Stmt(mk::Call(t, mk::Id(t, f), {})));
  return true;
}

REDLINE_TUNABLE(kManyPropertiesMaxCount, "generators.ManyProperties.max_count",
                2000, 1, 100000,
                "most properties added (counts are drawn around V8's limits: "
                "1020 descriptors, 1024 fast properties, "
                "the dictionary-mode and large-object thresholds)");

// A loop that gives an object N distinct property names, with N drawn around
// the points where V8 changes representation (1020 descriptors, 1024 fast
// properties, dictionary mode beyond, literal boilerplates with ever new
// names). The name is built from the loop counter, so every iteration brings
// a name the object has not seen.
GEN(ManyProperties) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view o = ctx.builder.FreshName("obj");
  std::string_view i = ctx.builder.FreshName("i");
  static const double kCounts[] = {8,    32,   127,  128,  129,  1000, 1019,
                                   1020, 1021, 1024, 1025, 1100, 2000};
  double count =
      std::min(kCounts[r.Index(sizeof(kCounts) / sizeof(kCounts[0]))],
               static_cast<double>(kManyPropertiesMaxCount.AsInt()));
  // "p" + i
  Expression* name = mk::Binary(t, BinaryOp::kAdd,
                                mk::Str(t, cg.PropertyName()), mk::Id(t, i));
  Expression* value = r.Coin() ? static_cast<Expression*>(mk::Id(t, i))
                               : cg.Value(JsType::kUnknown, 1);
  Statement* step = nullptr;
  switch (r.Index(3)) {
    case 0:
      // obj["p" + i] = v
      step = mk::ExprStmt(
          t, mk::Assign(t, mk::Index(t, mk::Id(t, o), name), value));
      break;
    case 1:
      // Object.defineProperty(obj, "p" + i, {value: v,
      // configurable/enumerable/writable at random})
      step = mk::ExprStmt(
          t, mk::CallMethod(
                 t, mk::Id(t, "Object"), "defineProperty",
                 {mk::Id(t, o), name,
                  mk::Object(
                      t, {mk::Prop(t, "value", value),
                          mk::Prop(t, "configurable", mk::Bool(t, r.Coin())),
                          mk::Prop(t, "enumerable", mk::Bool(t, r.Coin())),
                          mk::Prop(t, "writable", mk::Bool(t, r.Coin()))})}));
      break;
    default: {
      // eval("({" + ("p" + i) + ": 1})"): a literal with a name the parser has
      // never seen
      Expression* text =
          mk::Binary(t, BinaryOp::kAdd,
                     mk::Binary(t, BinaryOp::kAdd, mk::Str(t, "({"), name),
                     mk::Str(t, ": 1})"));
      step = mk::ExprStmt(
          t,
          mk::Assign(t, mk::Id(t, o), mk::Call(t, mk::Id(t, "eval"), {text})));
      break;
    }
  }
  Push(out, mk::Let(t, o, cg.ObjectLike()));
  ctx.Declare(o, JsType::kObject);
  Push(out, mk::CountedLoop(t, i, mk::Num(t, count), mk::Block(t, {step})));
  // Then use the result: a new literal (fresh boilerplate next to the full
  // caches), a property count or a key walk.
  switch (r.Index(3)) {
    case 0:
      Push(out, cg.Declare(cg.ObjectLiteral(1), JsType::kObject, "obj"));
      break;
    case 1:
      Push(out, cg.Declare(mk::Member(t,
                                      mk::CallMethod(t, mk::Id(t, "Object"),
                                                     "keys", {mk::Id(t, o)}),
                                      "length"),
                           JsType::kNumber));
      break;
    default:
      Push(out, cg.Stmt(mk::Assign(t, cg.Property(mk::Id(t, o)),
                                   cg.Value(JsType::kUnknown, 1))));
      break;
  }
  return true;
}

REDLINE_TUNABLE(
    kReturnOverrideMaxMembers, "generators.ReturnOverride.max_members", 3, 1,
    16, "most fields and methods the derived class installs on the receiver");
REDLINE_TUNABLE(kReturnOverridePrivateChance,
                "generators.ReturnOverride.private_chance", 0.6, 0, 1,
                "probability that a member is a private (#name) field or "
                "method rather than a public field");
REDLINE_TUNABLE(kReturnOverrideMaxReceivers,
                "generators.ReturnOverride.max_receivers", 4, 1, 32,
                "most receivers the class is applied to");
REDLINE_TUNABLE(kReturnOverrideRepeat, "generators.ReturnOverride.repeat", 20,
                1, 1000,
                "iterations of the loop that applies the class and its "
                "accessors to the receivers");
REDLINE_TUNABLE(kReturnOverrideDiscoveredChance,
                "generators.ReturnOverride.discovered_chance", 0.35, 0, 1,
                "probability that a receiver is an object produced by the "
                "target's own API (DiscoveredValue) rather than "
                "one of the generic kinds");

// A derived class whose base constructor returns its argument, so that the
// derived class's fields and private members are installed on that argument
// (a function, an array, a proxy, an API object, ...). Static methods give
// the program access to the private members afterwards.
GEN(ReturnOverride) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);
  std::string_view base = ctx.builder.FreshName("B");
  std::string_view cls = ctx.builder.FreshName("D");
  std::string_view arg = ctx.builder.FreshName("o");
  Statements base_body = {mk::Return(t, mk::Id(t, arg))};
  MethodDefinition* base_ctor = mk::ClassMethod(
      t, "constructor", mk::Function(t, "", {arg}, std::move(base_body)),
      MethodKind::kConstructor);
  Push(out, mk::ClassDecl(t, base, nullptr, {base_ctor}));
  ctx.Declare(base, JsType::kClass);
  GenContext nested = ctx.Nested(true, false, false, false);
  nested.syntax.strict = true;
  nested.syntax.in_method = true;
  CodeGen inner(nested);
  std::vector<Node*> members;
  struct Accessor {
    std::string_view name;
    int arity;
  };
  std::vector<Accessor> accessors;
  std::string_view param = ctx.builder.FreshName("o");
  std::string_view vparam = ctx.builder.FreshName("v");
  int n = 1 + r.SmallCount(kReturnOverrideMaxMembers.AsInt() - 1);
  for (int k = 0; k < n; k++) {
    std::string_view pname = ctx.builder.FreshName("p");
    if (!r.Chance(kReturnOverridePrivateChance())) {
      members.push_back(
          mk::ClassField(t, pname, inner.Value(JsType::kUnknown, 1)));
      continue;
    }
    if (r.Chance(0.25)) {
      // #m() { return v; }  static call(o) { return o.#m(); }
      Statements mbody = {mk::Return(t, inner.Value(JsType::kUnknown, 1))};
      members.push_back(mk::ClassMethod(
          t, mk::PrivateId(t, pname), mk::Function(t, "", {}, std::move(mbody)),
          MethodKind::kMethod, false));
      std::string_view call = ctx.builder.FreshName("call");
      Statements cbody = {mk::Return(
          t, mk::Call(t, mk::PrivateMember(t, mk::Id(t, param), pname), {}))};
      members.push_back(mk::ClassMethod(
          t, call, mk::Function(t, "", {param}, std::move(cbody)),
          MethodKind::kMethod, true));
      accessors.push_back({call, 1});
      continue;
    }
    // #p = v;  static get(o) { return o.#p; }  static set(o, v) { o.#p = v; }
    members.push_back(mk::ClassField(t, mk::PrivateId(t, pname),
                                     inner.Value(JsType::kUnknown, 1), false));
    std::string_view get = ctx.builder.FreshName("get");
    Statements gbody = {
        mk::Return(t, mk::PrivateMember(t, mk::Id(t, param), pname))};
    members.push_back(
        mk::ClassMethod(t, get, mk::Function(t, "", {param}, std::move(gbody)),
                        MethodKind::kMethod, true));
    accessors.push_back({get, 1});
    std::string_view set = ctx.builder.FreshName("set");
    Statements sbody = {mk::ExprStmt(
        t, mk::Assign(t, mk::PrivateMember(t, mk::Id(t, param), pname),
                      mk::Id(t, vparam)))};
    members.push_back(mk::ClassMethod(
        t, set, mk::Function(t, "", {param, vparam}, std::move(sbody)),
        MethodKind::kMethod, true));
    accessors.push_back({set, 2});
  }
  Push(out, mk::ClassDecl(t, cls, mk::Id(t, base), std::move(members)));
  ctx.Declare(cls, JsType::kClass);
  struct Receiver {
    Expression* value;
    const DiscoveredApi* source;  // the API it came from, if any
  };
  std::vector<Receiver> receivers;
  int nr = 1 + r.SmallCount(kReturnOverrideMaxReceivers.AsInt() - 1);
  for (int k = 0; k < nr; k++) {
    Expression* receiver = nullptr;
    const DiscoveredApi* source = nullptr;
    // Objects of the embedder's own kinds first (DiscoveredValue), then
    // the generic kinds; case 5 is DiscoveredValue too, for sessions
    // without introspection it falls through to a plain object.
    int kind = r.Chance(kReturnOverrideDiscoveredChance())
                   ? 5
                   : static_cast<int>(r.Index(10));
    switch (kind) {
      case 0:
        receiver = cg.Variable();
        break;
      case 1:
        receiver = cg.Callable();
        break;
      case 2:
        receiver = mk::New(t, mk::Id(t, "Proxy"),
                           {cg.ObjectLike(), mk::Object(t, {})});
        break;
      case 3:
        receiver = cg.Construct();
        break;
      case 4:
        receiver = mk::Id(t, "globalThis");
        break;
      case 5:
        // An object of the embedder's own kinds (see DiscoveredValue).
        receiver = DiscoveredValue(ctx, cg, &source);
        if (receiver == nullptr) {
          receiver = cg.ObjectLike();
        }
        break;
      case 6:
        // A function that already carries own properties:
        // Object.assign(f, {p: v}).
        receiver = mk::CallMethod(
            t, mk::Id(t, "Object"), "assign",
            {cg.Callable(),
             mk::Object(t, {mk::Prop(t, ctx.env.property_names()[r.Index(12)],
                                     cg.Value(JsType::kUnknown, 1))})});
        break;
      case 7:
        // A function made by the Function constructor, sloppy or strict:
        // new Function("'use strict'") has a map of its own.
        receiver = mk::New(t, mk::Id(t, "Function"),
                           {mk::Str(t, r.Coin() ? "'use strict'" : "")});
        break;
      case 8: {
        // The same, constructed with a foreign new.target (a visible
        // class, or a builtin): its prototype is taken from there, so
        // the function gets a map of yet another shape.
        Expression* new_target = cg.Variable(JsType::kClass);
        if (new_target == nullptr) {
          new_target = mk::Id(t, r.Pick({"Object", "Array", "Proxy", "Map",
                                         "Error", "Promise"}));
        }
        receiver = mk::CallMethod(
            t, mk::Id(t, "Reflect"), "construct",
            {mk::Id(t, "Function"),
             mk::Array(t, {mk::Str(t, r.Coin() ? "'use strict'" : "")}),
             new_target});
        break;
      }
      default:
        receiver = cg.ObjectLike();
        break;
    }
    if (receiver != nullptr) {
      receivers.push_back({receiver, source});
    }
  }
  // for (let i = 0; i < repeat; i++) { try { new D(r); D.get(r); D.set(r, v); }
  // catch (e) {} ... }
  Statements loop_body;
  auto guarded = [&](Expression* e) {
    return mk::Try(t, {mk::ExprStmt(t, e)}, ctx.builder.FreshName("e"), {});
  };
  auto accessor_calls = [&](std::string_view rv, Statements* steps) {
    for (const Accessor& a : accessors) {
      std::vector<Expression*> args = {mk::Id(t, rv)};
      if (a.arity > 1) {
        args.push_back(cg.Value(JsType::kUnknown, 1));
      }
      steps->push_back(
          guarded(mk::CallMethod(t, mk::Id(t, cls), a.name, std::move(args))));
    }
  };
  for (const Receiver& receiver : receivers) {
    std::string_view rv = ctx.builder.FreshName("r");
    loop_body.push_back(mk::Let(t, rv, nullptr));
    loop_body.push_back(guarded(mk::Assign(t, mk::Id(t, rv), receiver.value)));
    loop_body.push_back(guarded(mk::New(t, mk::Id(t, cls), {mk::Id(t, rv)})));
    accessor_calls(rv, &loop_body);
    // Hand the object to the embedder in between (an API that changes
    // its state, say) and read the members back afterwards; twice, so
    // that a state can be left and entered again.
    int rounds = static_cast<int>(r.Index(3));
    for (int round = 0; round < rounds; round++) {
      Expression* embedder =
          DiscoveredCallOn(ctx, cg, mk::Id(t, rv), receiver.source);
      if (embedder == nullptr) {
        break;
      }
      loop_body.push_back(guarded(embedder));
      accessor_calls(rv, &loop_body);
    }
  }
  std::string_view i = ctx.builder.FreshName("i");
  double repeat =
      static_cast<double>(r.Range(1, kReturnOverrideRepeat.AsInt()));
  Push(out, mk::CountedLoop(t, i, mk::Num(t, repeat),
                            mk::Block(t, std::move(loop_body))));
  return true;
}

// --- General Code Patterns
// ------------------------------------------------------

REDLINE_TUNABLE(kArrowStoreGCChance, "generators.ArrowStore.gc_chance", 0.5, 0,
                1, "chance to do GC");
REDLINE_TUNABLE(kArrowStoreArgumentsChance,
                "generators.ArrowStore.arguments_chance", 0.15, 0, 1,
                "chance to reference arguments");

GEN(ArrowStore) {
  Tree* t = T(ctx);
  Random& r = ctx.rng();

  GenContext f_ctx = ctx.Nested(true, false, false, false);
  CodeGen f_cg(f_ctx);
  std::vector<Statement*> f_body;

  int num_locals = static_cast<int>(r.Range(1, 4));
  f_body.reserve(static_cast<size_t>(num_locals));
  for (int i = 0; i < num_locals; i++) {
    f_body.push_back(f_cg.Declare(f_cg.Value(), JsType::kUnknown));
  }

  Expression* arrow_body = f_cg.Expression(2);
  Expression* arrow = mk::Arrow(t, {}, {mk::Return(t, arrow_body)});
  std::string_view arrow_name = f_ctx.builder.FreshName("arrow");
  f_body.push_back(mk::Let(t, arrow_name, arrow));

  std::string_view result_name = f_ctx.builder.FreshName("res");
  Expression* call_arrow = mk::Call(t, mk::Id(t, arrow_name), {});

  if (r.Chance(0.5)) {
    std::string_view wrap_name = f_ctx.builder.FreshName("wrap");
    std::string_view p = f_ctx.builder.FreshName("p");
    Statement* call_stmt = mk::Return(t, mk::Call(t, mk::Id(t, p), {}));
    if (r.Chance(0.5)) {
      call_stmt = mk::Try(t, {call_stmt}, "e", {});
    }
    f_body.push_back(mk::FunctionDecl(t, wrap_name, {p}, {call_stmt}));
    call_arrow = mk::Call(t, mk::Id(t, wrap_name), {mk::Id(t, arrow_name)});
  }

  f_body.push_back(mk::Let(t, result_name, call_arrow));
  f_ctx.Declare(result_name, JsType::kUnknown);

  int num_ops = static_cast<int>(r.Range(1, 4));
  for (int i = 0; i < num_ops; i++) {
    if (r.Chance(0.3)) {
      f_body.push_back(
          f_cg.Stmt(mk::Binary(t, CodeGen::RandomBinaryOp(r),
                               mk::Id(t, result_name), f_cg.Value())));
    } else if (r.Chance(0.5)) {
      f_body.push_back(
          f_cg.Stmt(mk::Assign(t, f_cg.Target(), mk::Id(t, result_name))));
    } else {
      f_body.push_back(
          f_cg.Stmt(f_cg.CallWithArguments(mk::Id(t, result_name), 2)));
    }
  }

  f_body.push_back(mk::Return(t, f_cg.Value()));

  std::string_view f_name = ctx.builder.FreshName("f");
  Push(out, mk::FunctionDecl(t, f_name, {}, std::move(f_body)));

  Statement* call_f = cg.Stmt(mk::Call(t, mk::Id(t, f_name), {}));
  if (r.Chance(0.5)) {
    Push(out, mk::CountedLoop(t, ctx.builder.FreshName("i"),
                              mk::Num(t, LoopCount(r)), call_f));
  } else {
    Push(out, cg.Stmt(mk::Native(t, "PrepareFunctionForOptimization",
                                 {mk::Id(t, f_name)})));
    Push(out, call_f);
    Push(out, cg.Stmt(mk::Native(t, "OptimizeFunctionOnNextCall",
                                 {mk::Id(t, f_name)})));
    Push(out, call_f);
  }

  return true;
}

REDLINE_TUNABLE(kDynamicFunctionStrictChance,
                "generators.DynamicFunctionTransition.strict_chance", 0.5, 0, 1,
                "chance to emit strict mode");

GEN(DynamicFunctionTransition) {
  Tree* t = T(ctx);
  Random& r = ctx.rng();

  Expression* body = cg.String();
  if (r.Chance(kDynamicFunctionStrictChance())) {
    body = mk::Binary(t, BinaryOp::kAdd, mk::Str(t, "'use strict'; "), body);
  }

  Expression* func_ctor = nullptr;
  if (r.Chance(0.3)) {
    func_ctor =
        mk::CallMethod(t, mk::Id(t, "Reflect"), "construct",
                       {mk::Id(t, "Function"), mk::Array(t, {body}),
                        r.Chance(0.5) ? cg.Value() : mk::Id(t, "Proxy")});
  } else if (r.Chance(0.5)) {
    func_ctor = mk::New(t, mk::Id(t, "Function"), {body});
  } else {
    func_ctor = mk::Call(t, mk::Id(t, "Function"), {body});
  }

  std::string_view fn_name = ctx.builder.FreshName("dynFn");
  Push(out, mk::Let(t, fn_name, func_ctor));
  ctx.Declare(fn_name, JsType::kFunction);

  if (r.Chance(0.5)) {
    std::string_view base_name = ctx.builder.FreshName("Base");
    std::string_view o_param = ctx.builder.FreshName("o");
    FunctionExpression* base_ctor =
        mk::Function(t, "", {o_param}, {mk::Return(t, mk::Id(t, fn_name))});
    Node* base_ctor_method =
        mk::ClassMethod(t, "constructor", base_ctor, MethodKind::kConstructor);
    Push(out, mk::ClassDecl(t, base_name, nullptr, {base_ctor_method}));

    std::string_view def_name = ctx.builder.FreshName("Derived");
    std::vector<Node*> class_members;
    int num_fields = static_cast<int>(r.Range(0, 3));
    for (int i = 0; i < num_fields; i++) {
      // A class field named "constructor" is a SyntaxError.
      std::string_view field = cg.PropertyName();
      if (field == "constructor") {
        field = ctx.builder.FreshName("f");
      }
      class_members.push_back(
          mk::ClassField(t, mk::Str(t, field), cg.Value(), false));
    }
    Push(out, mk::ClassDecl(t, def_name, mk::Id(t, base_name),
                            std::move(class_members)));

    Statement* create_derived =
        cg.Stmt(mk::New(t, mk::Id(t, def_name), {cg.Value()}));
    Push(out, mk::CountedLoop(t, ctx.builder.FreshName("i"),
                              mk::Num(t, LoopCount(r)), create_derived));
  } else {
    int num_props = static_cast<int>(r.Range(1, 4));
    for (int i = 0; i < num_props; i++) {
      Push(out,
           cg.Stmt(mk::Assign(t, cg.Property(mk::Id(t, fn_name)), cg.Value())));
    }
    Statement* call_fn = cg.Stmt(cg.CallWithArguments(mk::Id(t, fn_name), 2));
    Push(out, mk::CountedLoop(t, ctx.builder.FreshName("i"),
                              mk::Num(t, LoopCount(r)), call_fn));
  }

  return true;
}

REDLINE_TUNABLE(kCrossContextPrivateStoreLoopChance,
                "generators.CrossContextPrivateStore.loop_chance", 0.8, 0, 1,
                "chance to wrap in a loop");
REDLINE_TUNABLE(kCrossContextPrivateStoreAccessCheckedChance,
                "generators.CrossContextPrivateStore.access_checked_chance",
                0.05, 0, 1, "chance to use access-checked receiver");

GEN(CrossContextPrivateStore) {
  Tree* t = T(ctx);
  Random& r = ctx.rng();

  std::string_view cls_name = ctx.builder.FreshName("PrivClass");

  std::vector<Node*> members;
  int num_fields = static_cast<int>(r.Range(1, 4));
  std::vector<std::string_view> private_names;

  for (int i = 0; i < num_fields; i++) {
    std::string_view p_name = ctx.builder.FreshName("p");
    private_names.push_back(p_name);
    members.push_back(
        mk::ClassField(t, mk::PrivateId(t, p_name), cg.Value(), false));
  }

  int num_methods = static_cast<int>(r.Range(1, 3));
  std::vector<std::string_view> method_names;

  for (int i = 0; i < num_methods; i++) {
    std::string_view m_name = ctx.builder.FreshName("m");
    method_names.push_back(m_name);
    std::string_view obj_param = ctx.builder.FreshName("o");
    std::string_view val_param = ctx.builder.FreshName("v");

    std::string_view p_name = r.Pick(private_names);

    Statements body;
    Expression* target_obj =
        r.Chance(0.5) ? static_cast<Expression*>(mk::Id(t, obj_param))
                      : static_cast<Expression*>(mk::This(t));

    if (r.Chance(0.5)) {
      Statement* assign = mk::ExprStmt(
          t, mk::Assign(t, mk::PrivateMember(t, target_obj, p_name),
                        mk::Id(t, val_param)));
      if (r.Chance(0.5)) {
        body.push_back(mk::Try(t, {assign}, "e", {}));
      } else {
        body.push_back(assign);
      }
    } else {
      Statement* ret = mk::Return(t, mk::PrivateMember(t, target_obj, p_name));
      if (r.Chance(0.5)) {
        body.push_back(mk::Try(t, {ret}, "e", {mk::Return(t, cg.Value())}));
      } else {
        body.push_back(ret);
      }
    }
    members.push_back(mk::ClassMethod(
        t, m_name, mk::Function(t, "", {obj_param, val_param}, std::move(body)),
        MethodKind::kMethod, r.Chance(0.5)));
  }

  Expression* base_expr = nullptr;
  if (r.Chance(0.5)) {
    std::string_view base_name = ctx.builder.FreshName("Base");
    std::string_view base_o = ctx.builder.FreshName("o");
    FunctionExpression* base_ctor =
        mk::Function(t, "", {base_o},
                     {r.Chance(0.5) ? mk::Return(t, mk::Id(t, base_o))
                                    : mk::Return(t, cg.ObjectLiteral())});
    Node* base_ctor_method =
        mk::ClassMethod(t, "constructor", base_ctor, MethodKind::kConstructor);
    Push(out, mk::ClassDecl(t, base_name, nullptr, {base_ctor_method}));
    base_expr = mk::Id(t, base_name);
  }

  Push(out, mk::ClassDecl(t, cls_name, base_expr, std::move(members)));
  ctx.Declare(cls_name, JsType::kClass);

  Statements loop_body;
  std::string_view rec_name = ctx.builder.FreshName("rec");

  Expression* receiver = nullptr;
  int rec_type = static_cast<int>(r.Range(0, 8));
  if (rec_type == 0) {
    receiver = mk::CallMethod(t, mk::Member(t, mk::Id(t, "d8"), "test"),
                              "createAccessCheckedObject",
                              {mk::Bool(t, r.Chance(0.5))});
  } else if (rec_type == 1) {
    receiver = mk::New(t, mk::Id(t, "Proxy"),
                       {cg.ObjectLiteral(), cg.ObjectLiteral()});
  } else if (rec_type == 2) {
    receiver = cg.ObjectLike();
    if (!receiver) {
      receiver = cg.ObjectLiteral();
    }
  } else if (rec_type == 3) {
    receiver = mk::New(t, mk::Id(t, cls_name), {cg.Value()});
  } else if (rec_type == 4) {
    receiver = cg.ArrayLiteral();
  } else if (rec_type == 5) {
    receiver = cg.String();
  } else if (rec_type == 6) {
    receiver = cg.Number();
  } else {
    receiver = cg.Value();
  }

  loop_body.push_back(mk::Let(t, rec_name, receiver));

  if (r.Chance(0.5)) {
    loop_body.push_back(
        cg.Stmt(mk::New(t, mk::Id(t, cls_name), {mk::Id(t, rec_name)})));
  }

  int num_calls = static_cast<int>(r.Range(1, 4));
  for (int i = 0; i < num_calls; i++) {
    std::string_view m = r.Pick(method_names);
    loop_body.push_back(cg.Stmt(mk::CallMethod(
        t, mk::Id(t, cls_name), m, {mk::Id(t, rec_name), cg.Value()})));
  }

  Push(out,
       mk::CountedLoop(t, ctx.builder.FreshName("i"), mk::Num(t, LoopCount(r)),
                       mk::Block(t, std::move(loop_body))));

  return true;
}

GEN(UsingDecl) {
  Tree* t = T(ctx);
  Expression* init = cg.Value(JsType::kObject, 1);
  VariableDeclaration* d = mk::Declare(
      t, VariableKind::kUsing, ctx.builder.FreshName("using_var"), init);
  Push(out, mk::Block(t, {d}));
  return true;
}

GEN(AwaitUsingDecl) {
  Tree* t = T(ctx);
  Expression* init = cg.Value(JsType::kObject, 1);
  VariableDeclaration* d =
      mk::Declare(t, VariableKind::kAwaitUsing,
                  ctx.builder.FreshName("await_using_var"), init);
  Push(out, mk::Block(t, {d}));
  return true;
}

REDLINE_TUNABLE(kRawThrowConditionChance,
                "generators.RawThrow.condition_chance", 0.5, 0, 1,
                "chance to wrap throw in an if statement");

GEN(RawThrow) {
  Tree* t = T(ctx);
  Random& r = ctx.rng();
  Statement* th = mk::Throw(t, cg.Value(JsType::kUnknown, 1));
  if (r.Chance(kRawThrowConditionChance())) {
    Push(out, mk::If(t, cg.Boolean(), th));
  } else {
    Push(out, th);
  }
  return true;
}

GEN(NewTargetExpr) {
  Tree* t = T(ctx);
  Push(out, mk::Let(t, ctx.builder.FreshName("nt"), mk::NewTarget(t)));
  return true;
}

REDLINE_TUNABLE(kWorkerTerminateChance,
                "generators.WorkerSpawn.terminate_chance", 0.5, 0, 1,
                "chance to terminate the worker immediately");

GEN(WorkerSpawn) {
  Tree* t = T(ctx);
  Random& r = ctx.rng();

  std::string_view w_name = ctx.builder.FreshName("w");
  Expression* worker_src =
      mk::Str(t, "onmessage = function(e) { postMessage(e.data); }");
  Expression* worker_opts =
      mk::Object(t, {mk::Prop(t, "type", mk::Str(t, "string"))});
  Push(out,
       mk::Let(t, w_name,
               mk::New(t, mk::Id(t, "Worker"), {worker_src, worker_opts})));
  ctx.Declare(w_name, JsType::kUnknown);

  Push(out, cg.Stmt(mk::CallMethod(t, mk::Id(t, w_name), "postMessage",
                                   {cg.Value(JsType::kUnknown, 1)})));

  if (r.Chance(kWorkerTerminateChance())) {
    Push(out, cg.Stmt(mk::CallMethod(t, mk::Id(t, w_name), "terminate", {})));
  } else {
    // If we don't terminate, we can call getMessage to block and wait for the
    // message! But getMessage might hang if serialization fails! The
    // instructions say: "Keep runs deterministic and bounded: no long-lived
    // threads, and always terminate or await so programs finish within the 400
    // ms timeout." Let's just always terminate it! But wait! The tunable says
    // "chance to terminate immediately". If I don't terminate immediately, I
    // can do some operations and THEN terminate it. Actually, d8 cleans up
    // workers on exit, so if we just run, it terminates at the end. BUT to be
    // safe, I will just call terminate() at the end unconditionally, and maybe
    // getMessage() optionally.

    if (r.Coin()) {
      Push(out,
           cg.Stmt(mk::CallMethod(t, mk::Id(t, w_name), "getMessage", {})));
    }
    Push(out, cg.Stmt(mk::CallMethod(t, mk::Id(t, w_name), "terminate", {})));
  }

  return true;
}

#undef GEN

}  // namespace

// --- GeneratorSet
// -----------------------------------------------------------------

// static
std::unique_ptr<GeneratorSet> GeneratorSet::CreateDefault() {
  auto set = std::make_unique<GeneratorSet>();
  auto add = [&](const char* name, const char* description, double weight,
                 uint32_t requirements, GenFn fn) {
    set->generators_.push_back(std::make_unique<FnGenerator>(
        name, description, weight, requirements, fn));
    set->candidates_.push_back(set->generators_.back().get());
  };
  // Values.
  add("IntegerDecl", "let v = <integer>", 3, kNoRequirement, GenIntegerDecl);
  add("FloatDecl", "let v = <number>", 2, kNoRequirement, GenFloatDecl);
  add("StringDecl", "let s = <string>", 2, kNoRequirement, GenStringDecl);
  add("BooleanDecl", "let b = <boolean>", 1, kNoRequirement, GenBooleanDecl);
  add("BigIntDecl", "let n = <bigint>", 1, kNoRequirement, GenBigIntDecl);
  add("RegExpDecl", "let re = /.../", 1, kNoRequirement, GenRegExpDecl);
  add("NullishDecl", "let v = null | undefined", 0.5, kNoRequirement,
      GenNullishDecl);
  add("SymbolDecl", "let sym = Symbol(...)", 0.5, kNoRequirement,
      GenSymbolDecl);
  add("ArrayDecl", "let arr = [...]", 3, kNoRequirement, GenArrayDecl);
  add("ObjectDecl", "let obj = {...}", 3, kRecursive, GenObjectDecl);
  add("ConstructDecl", "let o = new Builtin(...)", 2, kNoRequirement,
      GenConstructDecl);
  add("TypedArrayDecl", "let ta = new XArray(...)", 1, kNoRequirement,
      GenTypedArrayDecl);
  add("ComputedDecl", "let v = <expression>", 3, kNoRequirement,
      GenComputedDecl);
  add("TemplateDecl", "let s = `...${}...`", 1, kNoRequirement,
      GenTemplateDecl);
  add("Destructuring", "let {a, b} = obj / let [x, y] = arr", 1, kNoRequirement,
      GenDestructuring);
  // Functions and classes.
  add("FunctionDecl", "function f(a, b) {...} f();", 4, kRecursive,
      GenFunctionDecl);
  add("ArrowDecl", "const f = (a) => {...}", 2, kRecursive, GenArrowDecl);
  add("ClassDecl", "class C extends ... {...}; new C()", 2, kRecursive,
      GenClassDecl);
  add("Iife", "(function() {...})()", 1, kRecursive, GenIife);
  add("Closure", "captured variable mutated by an arrow, called repeatedly", 1,
      kRecursive, GenClosure);
  // Operations.
  add("BinaryOp", "let v = a <op> b", 4, kNoRequirement, GenBinaryOp);
  add("Compare", "let b = a <cmp> b", 1.5, kNoRequirement, GenCompare);
  add("UnaryOp", "let v = <op> a / a++ / delete o.p", 1.5, kNoRequirement,
      GenUnaryOp);
  add("Update", "x++ / --o.p", 1.5, kNoRequirement, GenUpdate);
  add("Assignment", "target (op)= value", 3, kNoRequirement, GenAssignment);
  add("PropertyLoad", "let v = obj.p", 3, kNoRequirement, GenPropertyLoad);
  add("PropertyStore", "obj.p = v", 3, kNoRequirement, GenPropertyStore);
  add("ElementLoad", "let v = arr[i]", 2, kNoRequirement, GenElementLoad);
  add("ElementStore", "arr[i] = v", 2, kNoRequirement, GenElementStore);
  add("PropertyDelete", "delete obj.p", 1, kNoRequirement, GenPropertyDelete);
  add("MethodCall", "let v = obj.method(args)", 4, kNoRequirement,
      GenMethodCall);
  add("FunctionCall", "let v = f(args) / f.call / f.apply", 4, kNoRequirement,
      GenFunctionCall);
  add("BuiltinCall", "let v = Math.max(...) / JSON.stringify(...) ...", 3,
      kNoRequirement, GenBuiltinCall);
  add("Construct", "let o = new C(args)", 2, kNoRequirement, GenConstruct);
  add("TypeCheck", "typeof / instanceof / in", 1, kNoRequirement, GenTypeCheck);
  add("SpreadCall", "f(...arr)", 1, kNoRequirement, GenSpreadCall);
  add("StringOps", "string concatenation, indexing, cons/flat strings", 1.5,
      kNoRequirement, GenStringOps);
  add("TypedArrayOps", "typed array element access, detach, views", 1.5,
      kNoRequirement, GenTypedArrayOps);
  add("MapSetOps", "Map/Set set/get/has/delete", 1, kNoRequirement,
      GenMapSetOps);
  add("IntlOps",
      "Intl services with valid locales and options, locale-aware primitive "
      "methods",
      1, kNoRequirement, GenIntlOps);
  add("FrozenArrayOps", "sealed and frozen arrays with includes/indexOf/concat",
      1, kNoRequirement, GenFrozenArrayOps);
  add("TemporalOps", "Temporal with valid arguments and options", 1,
      kNoRequirement, GenTemporalOps);

  // Weight 0: the A/B experiment compares JavaScript-only runs (Fuzzilli runs
  // without --wasm). The `wasm` preset enables it.
  add("WasmModule",
      "WebAssembly module from building blocks, instantiated and called", 0,
      kNoRequirement, GenWasmModule);
  add("Iteration", "Array.from / [...x] / Object.keys", 1, kNoRequirement,
      GenIteration);
  add("DiscoveredApi", "new X()/X.m()/ns.f() for builtins the target reported",
      6, kNoRequirement, GenDiscoveredApi);
  add("PrototypeMethodCall", "Builtin.prototype.m.call(otherReceiver, ...)",
      1.5, kNoRequirement, GenPrototypeMethodCall);
  add("UnboundMethodCall",
      "const m = obj.method; m.call/apply/bind(other, ...)", 1.5,
      kNoRequirement, GenUnboundMethodCall);
  add("BuiltinOverwrite", "Builtin.prototype.m = function...; Math.max = ...",
      1, kRecursive, GenBuiltinOverwrite);
  add("ConstructorFunction",
      "function C(a, b) { if (!new.target) ...; this.x = a; }; new C()", 1.5,
      kRecursive, GenConstructorFunction);
  // Control flow.
  add("CountedLoop", "for (let i = 0; i < N; i++) {...}", 3, kRecursive,
      GenCountedLoop);
  add("WhileLoop", "bounded while / do-while", 1, kRecursive, GenWhileLoop);
  add("ForOf", "for (const e of iterable) {...}", 2, kRecursive, GenForOf);
  add("ForIn", "for (let k in obj) {...}", 1, kRecursive, GenForIn);
  add("IfElse", "if (cond) {...} else {...}", 3, kRecursive, GenIfElse);
  add("Switch", "switch (v) { case ...: }", 1, kRecursive, GenSwitch);
  add("TryCatch", "try {...} catch (e) {...} finally {...}", 2, kRecursive,
      GenTryCatch);
  add("Throw", "try { throw ... } catch {}", 0.5, kNoRequirement, GenThrow);
  add("LabeledBlock", "L: { ... break L; }", 0.5, kRecursive, GenLabeledBlock);
  add("Return", "if (cond) return v;", 1, kNeedsFunction, GenReturn);
  add("BreakContinue", "if (cond) break/continue;", 0.5, kNeedsBreak,
      GenBreakContinue);
  // JIT.
  add("JitOptimize",
      "%PrepareFunctionForOptimization(f); f(); "
      "%OptimizeFunctionOnNextCall(f); f();",
      4, kNoRequirement, GenJitOptimize);
  add("JitBaseline", "%CompileBaseline(f); f();", 0.5, kNoRequirement,
      GenJitBaseline);
  add("Deopt", "%DeoptimizeNow() / %DeoptimizeFunction(f)", 1, kNoRequirement,
      GenDeopt);
  add("OsrLoop", "loop with %OptimizeOsr()", 1, kRecursive, GenOsrLoop);
  add("NativeCall", "%Native(...) from the environment table", 1,
      kNoRequirement, GenNativeCall);
  add("Gc", "gc() / %SimulateNewspaceFull()", 0.5, kNoRequirement, GenGc);
  // Object model.
  add("PrototypeSet", "Object.setPrototypeOf / __proto__ =", 1, kNoRequirement,
      GenPrototypeSet);
  add("DefineProperty", "Object.defineProperty(obj, k, {get/value...})", 1.5,
      kRecursive, GenDefineProperty);
  add("ValueOfOverride",
      "obj.valueOf / toString / Symbol.toPrimitive = () => ...", 1,
      kNoRequirement, GenValueOfOverride);
  add("ElementsKind", "array elements kind transitions", 2, kNoRequirement,
      GenElementsKind);
  add("FreezeSeal", "Object.freeze/seal/preventExtensions", 0.5, kNoRequirement,
      GenFreezeSeal);
  add("ProxyDecl", "let p = new Proxy(target, {traps})", 1, kRecursive,
      GenProxyDecl);
  add("With", "with (obj) {...}", 0.3, kNeedsSloppy | kRecursive, GenWith);
  add("ShapeChange", "add/delete properties on an object", 1.5, kNoRequirement,
      GenShapeChange);
  add("Eval", "eval / indirect eval of a snippet", 0.3, kNoRequirement,
      GenEval);
  // Stress patterns.
  add("Recursion",
      "f calls itself under try/catch, bounded or until a caught stack "
      "overflow",
      1, kRecursive, GenRecursion);
  add("WrapperChain",
      "w = wrap(w) N times (bind, Object.create, [w], proxy, string), then "
      "consumed",
      1, kNoRequirement, GenWrapperChain);
  add("ManyProperties",
      "N distinct property names via stores, defineProperty or eval'd literals",
      1, kNoRequirement, GenManyProperties);
  add("ReturnOverride",
      "class B { constructor(o) { return o; } } class D extends B { #p; static "
      "get(o) {...} }; new D(x)",
      1.5, kRecursive, GenReturnOverride);
  add("ArgumentsUse", "arguments.length / arguments[i] / [...arguments]", 0.5,
      kNeedsArguments, GenArgumentsUse);
  add("ThisUse", "this.p load/store", 1, kNeedsThis, GenThisUse);
  // Async and generators.
  add("Await", "let v = await ...", 2, kNeedsAsync, GenAwait);
  add("Yield", "let v = yield ...", 2, kNeedsGenerator, GenYield);
  add("AsyncIife", "(async () => { ... await ... })()", 1, kRecursive,
      GenAsyncIife);
  add("GeneratorIterate",
      "function* g() {...}; for (const x of g()) / it.next()", 1, kRecursive,
      GenGeneratorIterate);
  add("PromiseThen", "promise.then(v => ..., e => ...)", 1, kRecursive,
      GenPromiseThen);
  add("WorkerSpawn", "spawn a WebWorker", 1.0, kNoRequirement, GenWorkerSpawn);
  add("ArrowStore", "store an object or smi to an arrow function", 1.0,
      kNoRequirement, GenArrowStore);
  add("DynamicFunctionTransition", "dynamic function with map transition", 1.0,
      kNoRequirement, GenDynamicFunctionTransition);
  add("CrossContextPrivateStore", "cross-context private field store", 1.0,
      kNoRequirement, GenCrossContextPrivateStore);
  add("UsingDecl", "using x = ...", 1.0, kNoRequirement, GenUsingDecl);
  add("AwaitUsingDecl", "await using x = ...", 1.0, kNeedsAsync,
      GenAwaitUsingDecl);
  add("RawThrow", "throw ...", 0.5, kNoRequirement, GenRawThrow);
  add("NewTargetExpr", "let nt = new.target", 1.0, kNeedsArguments,
      GenNewTargetExpr);
  std::string error;
  if (!set->SetSelector(
          std::make_unique<WeightedSelector<Generator>>(
              "generator", std::vector<std::pair<std::string, double>>{}),
          &error)) {
    REDLINE_ERROR("generators", "default generator strategy: %s",
                  error.c_str());
    return nullptr;
  }
  return set;
}

bool GeneratorSet::SetSelector(std::unique_ptr<Selector<Generator>> selector,
                               std::string* error) {
  if (!selector->Initialize(candidates_, error)) {
    return false;
  }
  selector_ = std::move(selector);
  return true;
}

Generator* GeneratorSet::Find(std::string_view name) const {
  for (const auto& g : generators_) {
    if (g->name() == name) {
      return g.get();
    }
  }
  return nullptr;
}

std::vector<std::string> GeneratorSet::Names() const {
  std::vector<std::string> names;
  names.reserve(generators_.size());
  for (const auto& g : generators_) {
    names.push_back(g->name());
  }
  return names;
}

Generator* GeneratorSet::Pick(const GenContext& ctx) const {
  size_t index =
      selector_->Pick(candidates_, ctx.builder.rng(),
                      [&](const Generator& g) { return g.Applicable(ctx); });
  return index == Selector<Generator>::kNoChoice ? nullptr : candidates_[index];
}

// --- selection strategies for generators -----------------------------------
//
// To add one: derive from Selector<Generator>, then add a Register() line
// below. Parameters are declared in the Entry so that the configuration is
// validated before the fuzzer starts.

template <>
void RegisterBuiltinSelectors<Generator>(
    SelectorRegistry<Generator>& registry) {
  registry.Register(
      {std::string(AdaptiveSelector<Generator>::kName),
       "weighted draw whose weights follow each generator's recent yield",
       AdaptiveSelector<Generator>::Specs(),
       [](const JsonValue& params,
          std::string* error) -> std::unique_ptr<Selector<Generator>> {
         std::vector<std::pair<std::string, double>> overrides;
         if (!ReadWeightOverrides(params, &overrides, error)) {
           return nullptr;
         }
         AdaptiveSelector<Generator>::Settings p =
             AdaptiveSelector<Generator>::ReadParams(params);
         if (!AdaptiveSelector<Generator>::Validate(p, error)) {
           return nullptr;
         }
         return std::make_unique<AdaptiveSelector<Generator>>(
             "generator", std::move(overrides), p);
       }});
  registry.Register({std::string(WeightedSelector<Generator>::kName),
                     "one weighted random draw among the applicable generators",
                     WeightedSelector<Generator>::Specs(),
                     [](const JsonValue& params, std::string* error)
                         -> std::unique_ptr<Selector<Generator>> {
                       std::vector<std::pair<std::string, double>> overrides;
                       if (!ReadWeightOverrides(params, &overrides, error)) {
                         return nullptr;
                       }
                       // Names are checked against the generators in
                       // Initialize().
                       return std::make_unique<WeightedSelector<Generator>>(
                           "generator", std::move(overrides));
                     }});
  registry.SetDefault(WeightedSelector<Generator>::kName);
}

int GeneratorSet::Generate(GenContext& ctx, int count,
                           std::vector<fast::Statement*>* out) const {
  int produced = 0;
  std::vector<fast::Statement*>* saved_out = ctx.out;
  for (int i = 0; i < count; i++) {
    Generator* g = Pick(ctx);
    if (g == nullptr) {
      break;
    }
    g->stats().invocations++;
    size_t before = out->size();
    ctx.out = out;
    if (g->Generate(ctx, out) && out->size() > before) {
      g->stats().produced++;
      produced++;
    } else {
      out->resize(before);
    }
  }
  ctx.out = saved_out;
  return produced;
}

std::vector<const Component*> GeneratorSet::Components() const {
  std::vector<const Component*> components;
  components.reserve(generators_.size());
  for (const auto& g : generators_) {
    components.push_back(g.get());
  }
  return components;
}

GenContext MakeGenContext(Builder& builder, const Builder::Slot& slot,
                          const GeneratorSet& generators,
                          const Environment& env) {
  GenContext ctx(builder, generators, env);
  ctx.slot = slot;
  ctx.syntax = builder.ContextAtSlot(slot);
  ctx.visible = builder.VisibleAtSlot(slot);
  return ctx;
}

}  // namespace redline

namespace redline {
namespace {
// One list for each thread. The list is never deleted, so that the thread has
// no destructor to run when it exits.
std::vector<Generator*>& UsedGenerators() {
  thread_local std::vector<Generator*>* used = new std::vector<Generator*>();
  return *used;
}
}  // namespace
void BeginGeneratorUsage() { UsedGenerators().clear(); }
void NoteGeneratorUsed(Generator* g) {
  std::vector<Generator*>& used = UsedGenerators();
  if (std::find(used.begin(), used.end(), g) == used.end()) {
    used.push_back(g);
  }
}
std::vector<Generator*> GeneratorsUsed() { return UsedGenerators(); }
}  // namespace redline
