// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FAST: the fuzzer's JavaScript AST.
//
// The tree is deliberately shaped like ESTree (the AST format of acorn,
// babel, esprima, ...) so that dumps are comparable with those tools and so
// that the literature on AST mutation applies directly. Differences:
//   * literals are split into one node kind per literal type,
//   * `%RuntimeFunction(...)` calls (--allow-natives-syntax) are their own
//     kind,
//   * parentheses are not represented; the printer re-derives them from
//     precedence.
//
// Nodes are plain structs allocated in a per-program Arena; child pointers
// are raw and owned by the arena. Each node has a per-program id (stable for
// the lifetime of the program; the shell and attribution logs refer to nodes
// by id) and the [start, end) byte range it was parsed from (-1 when the node
// was synthesized by the fuzzer).

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_AST_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_AST_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/fast/arena.h"

namespace redline {
namespace fast {

// V(kind, class)
#define FAST_STATEMENT_KIND_LIST(V) \
  V(ExpressionStatement)            \
  V(BlockStatement)                 \
  V(EmptyStatement)                 \
  V(DebuggerStatement)              \
  V(WithStatement)                  \
  V(ReturnStatement)                \
  V(LabeledStatement)               \
  V(BreakStatement)                 \
  V(ContinueStatement)              \
  V(IfStatement)                    \
  V(SwitchStatement)                \
  V(ThrowStatement)                 \
  V(TryStatement)                   \
  V(WhileStatement)                 \
  V(DoWhileStatement)               \
  V(ForStatement)                   \
  V(ForInStatement)                 \
  V(ForOfStatement)                 \
  V(FunctionDeclaration)            \
  V(VariableDeclaration)            \
  V(ClassDeclaration)

#define FAST_EXPRESSION_KIND_LIST(V) \
  V(Identifier)                      \
  V(PrivateIdentifier)               \
  V(NullLiteral)                     \
  V(BooleanLiteral)                  \
  V(NumericLiteral)                  \
  V(BigIntLiteral)                   \
  V(StringLiteral)                   \
  V(RegExpLiteral)                   \
  V(ThisExpression)                  \
  V(Super)                           \
  V(ArrayExpression)                 \
  V(ObjectExpression)                \
  V(FunctionExpression)              \
  V(ArrowFunctionExpression)         \
  V(ClassExpression)                 \
  V(UnaryExpression)                 \
  V(UpdateExpression)                \
  V(BinaryExpression)                \
  V(LogicalExpression)               \
  V(AssignmentExpression)            \
  V(MemberExpression)                \
  V(ConditionalExpression)           \
  V(CallExpression)                  \
  V(NewExpression)                   \
  V(SequenceExpression)              \
  V(YieldExpression)                 \
  V(AwaitExpression)                 \
  V(TemplateLiteral)                 \
  V(TaggedTemplateExpression)        \
  V(MetaProperty)                    \
  V(ImportExpression)                \
  V(ChainExpression)                 \
  V(SpreadElement)                   \
  V(ObjectPattern)                   \
  V(ArrayPattern)                    \
  V(RestElement)                     \
  V(AssignmentPattern)               \
  V(NativeCallExpression)

#define FAST_OTHER_KIND_LIST(V) \
  V(Program)                    \
  V(VariableDeclarator)         \
  V(SwitchCase)                 \
  V(CatchClause)                \
  V(Property)                   \
  V(ClassBody)                  \
  V(MethodDefinition)           \
  V(PropertyDefinition)         \
  V(StaticBlock)                \
  V(TemplateElement)

#define FAST_NODE_KIND_LIST(V) \
  FAST_STATEMENT_KIND_LIST(V)  \
  FAST_EXPRESSION_KIND_LIST(V) \
  FAST_OTHER_KIND_LIST(V)

enum class NodeKind : uint8_t {
#define DECLARE_KIND(name) k##name,
  FAST_NODE_KIND_LIST(DECLARE_KIND)
#undef DECLARE_KIND
};

#define COUNT_KIND(name) \
  +1  // NOLINT(bugprone-macro-parentheses): X-macro summand, `(+1)` would not
      // chain
constexpr int kNumNodeKinds = 0 FAST_NODE_KIND_LIST(COUNT_KIND);
#undef COUNT_KIND

const char* NodeKindName(NodeKind kind);
bool IsStatementKind(NodeKind kind);
bool IsExpressionKind(NodeKind kind);

#define FORWARD_DECLARE(name) struct name;
FAST_NODE_KIND_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

// ---------------------------------------------------------------------------
// Operators.

#define FAST_UNARY_OP_LIST(V) \
  V(kMinus, "-")              \
  V(kPlus, "+")               \
  V(kNot, "!")                \
  V(kBitNot, "~")             \
  V(kTypeof, "typeof")        \
  V(kVoid, "void")            \
  V(kDelete, "delete")

#define FAST_UPDATE_OP_LIST(V) \
  V(kIncrement, "++")          \
  V(kDecrement, "--")

// Ordered by precedence group; see BinaryPrecedence().
#define FAST_BINARY_OP_LIST(V) \
  V(kEq, "==")                 \
  V(kNotEq, "!=")              \
  V(kStrictEq, "===")          \
  V(kStrictNotEq, "!==")       \
  V(kLessThan, "<")            \
  V(kLessThanEq, "<=")         \
  V(kGreaterThan, ">")         \
  V(kGreaterThanEq, ">=")      \
  V(kShiftLeft, "<<")          \
  V(kShiftRight, ">>")         \
  V(kShiftRightLogical, ">>>") \
  V(kAdd, "+")                 \
  V(kSub, "-")                 \
  V(kMul, "*")                 \
  V(kDiv, "/")                 \
  V(kMod, "%")                 \
  V(kExp, "**")                \
  V(kBitOr, "|")               \
  V(kBitXor, "^")              \
  V(kBitAnd, "&")              \
  V(kIn, "in")                 \
  V(kInstanceof, "instanceof")

#define FAST_LOGICAL_OP_LIST(V) \
  V(kOr, "||")                  \
  V(kAnd, "&&")                 \
  V(kNullish, "??")

#define FAST_ASSIGN_OP_LIST(V)        \
  V(kAssign, "=")                     \
  V(kAddAssign, "+=")                 \
  V(kSubAssign, "-=")                 \
  V(kMulAssign, "*=")                 \
  V(kDivAssign, "/=")                 \
  V(kModAssign, "%=")                 \
  V(kExpAssign, "**=")                \
  V(kShiftLeftAssign, "<<=")          \
  V(kShiftRightAssign, ">>=")         \
  V(kShiftRightLogicalAssign, ">>>=") \
  V(kBitOrAssign, "|=")               \
  V(kBitXorAssign, "^=")              \
  V(kBitAndAssign, "&=")              \
  V(kOrAssign, "||=")                 \
  V(kAndAssign, "&&=")                \
  V(kNullishAssign, "??=")

#define DECLARE_ENUM(name, text) name,
enum class UnaryOp : uint8_t { FAST_UNARY_OP_LIST(DECLARE_ENUM) };
enum class UpdateOp : uint8_t { FAST_UPDATE_OP_LIST(DECLARE_ENUM) };
enum class BinaryOp : uint8_t { FAST_BINARY_OP_LIST(DECLARE_ENUM) };
enum class LogicalOp : uint8_t { FAST_LOGICAL_OP_LIST(DECLARE_ENUM) };
enum class AssignOp : uint8_t { FAST_ASSIGN_OP_LIST(DECLARE_ENUM) };
#undef DECLARE_ENUM

const char* OperatorText(UnaryOp op);
const char* OperatorText(UpdateOp op);
const char* OperatorText(BinaryOp op);
const char* OperatorText(LogicalOp op);
const char* OperatorText(AssignOp op);

// Binary operator precedence (higher binds tighter), 1 = `??`, 2 = `||`,
// 3 = `&&`, 4 = `|`, 5 = `^`, 6 = `&`, 7 = equality, 8 = relational,
// 9 = shift, 10 = additive, 11 = multiplicative, 12 = `**`.
int BinaryPrecedence(BinaryOp op);
int LogicalPrecedence(LogicalOp op);

enum class VariableKind : uint8_t { kVar, kLet, kConst, kUsing, kAwaitUsing };
const char* VariableKindText(VariableKind kind);

enum class PropertyKind : uint8_t { kInit, kGet, kSet };
enum class MethodKind : uint8_t { kConstructor, kMethod, kGet, kSet };
enum class MetaPropertyKind : uint8_t { kNewTarget, kImportMeta };

// ---------------------------------------------------------------------------
// Nodes.

struct Node {
  explicit Node(NodeKind k) : kind(k) {}

  NodeKind kind;
  // Set by the parser for directive prologues / strictness tracking etc.
  uint8_t flags = 0;
  uint32_t id = 0;
  // Byte offsets into the source this node was parsed from, or -1.
  int32_t start = -1;
  int32_t end = -1;

  bool Is(NodeKind k) const { return kind == k; }
  template <typename T>
  bool Is() const {
    return kind == T::kKind;
  }
  template <typename T>
  T* As() {
    return kind == T::kKind ? static_cast<T*>(this) : nullptr;
  }
  template <typename T>
  const T* As() const {
    return kind == T::kKind ? static_cast<const T*>(this) : nullptr;
  }
  template <typename T>
  T* Cast() {
    return static_cast<T*>(this);
  }
  template <typename T>
  const T* Cast() const {
    return static_cast<const T*>(this);
  }
  bool IsStatement() const { return IsStatementKind(kind); }
  bool IsExpression() const { return IsExpressionKind(kind); }
  bool IsPattern() const;   // ObjectPattern/ArrayPattern/AssignmentPattern/Rest
  bool IsFunction() const;  // FunctionDeclaration/FunctionExpression/Arrow
  bool IsClass() const;     // ClassDeclaration/ClassExpression
  const char* kind_name() const { return NodeKindName(kind); }
};

struct Statement : Node {
  using Node::Node;
};
struct Expression : Node {
  using Node::Node;
};

#define NODE_BOILERPLATE(name, base)                   \
  static constexpr NodeKind kKind = NodeKind::k##name; \
  name() : base(NodeKind::k##name) {}

// --- Program ---------------------------------------------------------------

struct Program : Node {
  NODE_BOILERPLATE(Program, Node)
  std::vector<Statement*> body;
  bool strict = false;  // has a "use strict" directive
};

// --- Statements ------------------------------------------------------------

struct ExpressionStatement : Statement {
  NODE_BOILERPLATE(ExpressionStatement, Statement)
  Expression* expression = nullptr;
  // For directive prologue entries (e.g. "use strict"): the raw string
  // literal text without quotes. Empty for ordinary statements.
  std::string_view directive;
  bool is_directive = false;
};

struct BlockStatement : Statement {
  NODE_BOILERPLATE(BlockStatement, Statement)
  std::vector<Statement*> body;
};

struct EmptyStatement : Statement {
  NODE_BOILERPLATE(EmptyStatement, Statement)
};

struct DebuggerStatement : Statement {
  NODE_BOILERPLATE(DebuggerStatement, Statement)
};

struct WithStatement : Statement {
  NODE_BOILERPLATE(WithStatement, Statement)
  Expression* object = nullptr;
  Statement* body = nullptr;
};

struct ReturnStatement : Statement {
  NODE_BOILERPLATE(ReturnStatement, Statement)
  Expression* argument = nullptr;  // nullable
};

struct LabeledStatement : Statement {
  NODE_BOILERPLATE(LabeledStatement, Statement)
  Identifier* label = nullptr;
  Statement* body = nullptr;
};

struct BreakStatement : Statement {
  NODE_BOILERPLATE(BreakStatement, Statement)
  Identifier* label = nullptr;  // nullable
};

struct ContinueStatement : Statement {
  NODE_BOILERPLATE(ContinueStatement, Statement)
  Identifier* label = nullptr;  // nullable
};

struct IfStatement : Statement {
  NODE_BOILERPLATE(IfStatement, Statement)
  Expression* test = nullptr;
  Statement* consequent = nullptr;
  Statement* alternate = nullptr;  // nullable
};

struct SwitchCase : Node {
  NODE_BOILERPLATE(SwitchCase, Node)
  Expression* test = nullptr;  // nullptr for `default:`
  std::vector<Statement*> consequent;
};

struct SwitchStatement : Statement {
  NODE_BOILERPLATE(SwitchStatement, Statement)
  Expression* discriminant = nullptr;
  std::vector<SwitchCase*> cases;
};

struct ThrowStatement : Statement {
  NODE_BOILERPLATE(ThrowStatement, Statement)
  Expression* argument = nullptr;
};

struct CatchClause : Node {
  NODE_BOILERPLATE(CatchClause, Node)
  Expression* param = nullptr;  // Identifier or pattern; nullptr if omitted
  BlockStatement* body = nullptr;
};

struct TryStatement : Statement {
  NODE_BOILERPLATE(TryStatement, Statement)
  BlockStatement* block = nullptr;
  CatchClause* handler = nullptr;       // nullable
  BlockStatement* finalizer = nullptr;  // nullable
};

struct WhileStatement : Statement {
  NODE_BOILERPLATE(WhileStatement, Statement)
  Expression* test = nullptr;
  Statement* body = nullptr;
};

struct DoWhileStatement : Statement {
  NODE_BOILERPLATE(DoWhileStatement, Statement)
  Statement* body = nullptr;
  Expression* test = nullptr;
};

struct ForStatement : Statement {
  NODE_BOILERPLATE(ForStatement, Statement)
  Node* init = nullptr;          // VariableDeclaration or Expression; nullable
  Expression* test = nullptr;    // nullable
  Expression* update = nullptr;  // nullable
  Statement* body = nullptr;
};

struct ForInStatement : Statement {
  NODE_BOILERPLATE(ForInStatement, Statement)
  Node* left = nullptr;  // VariableDeclaration or pattern/Identifier/Member
  Expression* right = nullptr;
  Statement* body = nullptr;
};

struct ForOfStatement : Statement {
  NODE_BOILERPLATE(ForOfStatement, Statement)
  Node* left = nullptr;
  Expression* right = nullptr;
  Statement* body = nullptr;
  bool is_await = false;
};

// Fields shared by all function-like nodes. Not a Node itself.
struct FunctionData {
  Identifier* name = nullptr;             // nullable (not Node::id)
  std::vector<Expression*> params;        // Identifier / patterns / RestElement
  BlockStatement* body = nullptr;         // nullptr for concise arrow bodies
  Expression* expression_body = nullptr;  // concise arrow body
  bool is_async = false;
  bool is_generator = false;
  bool is_arrow = false;
  bool strict = false;  // body (or enclosing code) is strict
};

struct FunctionDeclaration : Statement, FunctionData {
  NODE_BOILERPLATE(FunctionDeclaration, Statement)
};

struct VariableDeclarator : Node {
  NODE_BOILERPLATE(VariableDeclarator, Node)
  Expression* target = nullptr;  // Identifier or pattern
  Expression* init = nullptr;    // nullable
};

struct VariableDeclaration : Statement {
  NODE_BOILERPLATE(VariableDeclaration, Statement)
  VariableKind declaration_kind = VariableKind::kVar;
  std::vector<VariableDeclarator*> declarations;
};

// Fields shared by class declarations and expressions.
struct ClassData {
  Identifier* name = nullptr;         // nullable (not Node::id)
  Expression* super_class = nullptr;  // nullable
  ClassBody* body = nullptr;
};

struct ClassDeclaration : Statement, ClassData {
  NODE_BOILERPLATE(ClassDeclaration, Statement)
};

// --- Expressions -----------------------------------------------------------

struct Identifier : Expression {
  NODE_BOILERPLATE(Identifier, Expression)
  std::string_view name;  // cooked (escapes resolved), UTF-8
};

struct PrivateIdentifier : Expression {
  NODE_BOILERPLATE(PrivateIdentifier, Expression)
  std::string_view name;  // without the leading '#'
};

struct NullLiteral : Expression {
  NODE_BOILERPLATE(NullLiteral, Expression)
};

struct BooleanLiteral : Expression {
  NODE_BOILERPLATE(BooleanLiteral, Expression)
  bool value = false;
};

struct NumericLiteral : Expression {
  NODE_BOILERPLATE(NumericLiteral, Expression)
  double value = 0;
  std::string_view raw;  // source text when parsed, empty when synthesized
};

struct BigIntLiteral : Expression {
  NODE_BOILERPLATE(BigIntLiteral, Expression)
  std::string_view raw;  // including the trailing 'n'
};

struct StringLiteral : Expression {
  NODE_BOILERPLATE(StringLiteral, Expression)
  std::string value;     // cooked, WTF-8 (lone surrogates encoded as 3 bytes)
  std::string_view raw;  // source text including quotes, or empty
};

struct RegExpLiteral : Expression {
  NODE_BOILERPLATE(RegExpLiteral, Expression)
  std::string_view pattern;
  std::string_view regexp_flags;
};

struct ThisExpression : Expression {
  NODE_BOILERPLATE(ThisExpression, Expression)
};

struct Super : Expression {
  NODE_BOILERPLATE(Super, Expression)
};

struct ArrayExpression : Expression {
  NODE_BOILERPLATE(ArrayExpression, Expression)
  std::vector<Expression*> elements;  // nullptr entries are holes
};

struct Property : Node {
  NODE_BOILERPLATE(Property, Node)
  Expression* key = nullptr;    // Identifier, String/Numeric/BigInt literal,
                                // PrivateIdentifier, or any expr if computed
  Expression* value = nullptr;  // FunctionExpression for methods/accessors
  PropertyKind property_kind = PropertyKind::kInit;
  bool computed = false;
  bool shorthand = false;
  bool method = false;
};

struct ObjectExpression : Expression {
  NODE_BOILERPLATE(ObjectExpression, Expression)
  std::vector<Node*> properties;  // Property or SpreadElement
};

struct FunctionExpression : Expression, FunctionData {
  NODE_BOILERPLATE(FunctionExpression, Expression)
};

struct ArrowFunctionExpression : Expression, FunctionData {
  NODE_BOILERPLATE(ArrowFunctionExpression, Expression)
};

struct ClassExpression : Expression, ClassData {
  NODE_BOILERPLATE(ClassExpression, Expression)
};

struct UnaryExpression : Expression {
  NODE_BOILERPLATE(UnaryExpression, Expression)
  UnaryOp op = UnaryOp::kMinus;
  Expression* argument = nullptr;
};

struct UpdateExpression : Expression {
  NODE_BOILERPLATE(UpdateExpression, Expression)
  UpdateOp op = UpdateOp::kIncrement;
  bool prefix = false;
  Expression* argument = nullptr;
};

struct BinaryExpression : Expression {
  NODE_BOILERPLATE(BinaryExpression, Expression)
  BinaryOp op = BinaryOp::kAdd;
  Expression* left = nullptr;  // PrivateIdentifier for `#x in obj`
  Expression* right = nullptr;
};

struct LogicalExpression : Expression {
  NODE_BOILERPLATE(LogicalExpression, Expression)
  LogicalOp op = LogicalOp::kAnd;
  Expression* left = nullptr;
  Expression* right = nullptr;
};

struct AssignmentExpression : Expression {
  NODE_BOILERPLATE(AssignmentExpression, Expression)
  AssignOp op = AssignOp::kAssign;
  Expression* left = nullptr;  // Identifier, MemberExpression or pattern
  Expression* right = nullptr;
};

struct MemberExpression : Expression {
  NODE_BOILERPLATE(MemberExpression, Expression)
  Expression* object = nullptr;    // may be Super
  Expression* property = nullptr;  // Identifier/PrivateIdentifier or expr
  bool computed = false;
  bool optional = false;  // `a?.b`
};

struct ConditionalExpression : Expression {
  NODE_BOILERPLATE(ConditionalExpression, Expression)
  Expression* test = nullptr;
  Expression* consequent = nullptr;
  Expression* alternate = nullptr;
};

struct CallExpression : Expression {
  NODE_BOILERPLATE(CallExpression, Expression)
  Expression* callee = nullptr;        // may be Super
  std::vector<Expression*> arguments;  // SpreadElement allowed
  bool optional = false;               // `f?.()`
};

struct NewExpression : Expression {
  NODE_BOILERPLATE(NewExpression, Expression)
  Expression* callee = nullptr;
  std::vector<Expression*> arguments;
};

struct SequenceExpression : Expression {
  NODE_BOILERPLATE(SequenceExpression, Expression)
  std::vector<Expression*> expressions;
};

struct YieldExpression : Expression {
  NODE_BOILERPLATE(YieldExpression, Expression)
  Expression* argument = nullptr;  // nullable
  bool delegate = false;
};

struct AwaitExpression : Expression {
  NODE_BOILERPLATE(AwaitExpression, Expression)
  Expression* argument = nullptr;
};

struct TemplateElement : Node {
  NODE_BOILERPLATE(TemplateElement, Node)
  std::string_view raw;
  std::string cooked;
  bool cooked_valid = true;  // false for invalid escapes in tagged templates
  bool tail = false;
};

struct TemplateLiteral : Expression {
  NODE_BOILERPLATE(TemplateLiteral, Expression)
  std::vector<TemplateElement*>
      quasis;  // quasis.size() == expressions.size() + 1
  std::vector<Expression*> expressions;
};

struct TaggedTemplateExpression : Expression {
  NODE_BOILERPLATE(TaggedTemplateExpression, Expression)
  Expression* tag = nullptr;
  TemplateLiteral* quasi = nullptr;
};

struct MetaProperty : Expression {
  NODE_BOILERPLATE(MetaProperty, Expression)
  MetaPropertyKind meta_kind = MetaPropertyKind::kNewTarget;
};

struct ImportExpression : Expression {
  NODE_BOILERPLATE(ImportExpression, Expression)
  Expression* source = nullptr;
  Expression* options = nullptr;  // nullable
  // `import.defer(...)` (deferred import evaluation proposal).
  bool is_defer = false;
};

// Wraps an optional chain (`a?.b.c`) so that short-circuiting extent and
// parenthesization are explicit.
struct ChainExpression : Expression {
  NODE_BOILERPLATE(ChainExpression, Expression)
  Expression* expression = nullptr;  // MemberExpression or CallExpression
};

struct SpreadElement : Expression {
  NODE_BOILERPLATE(SpreadElement, Expression)
  Expression* argument = nullptr;
};

struct ObjectPattern : Expression {
  NODE_BOILERPLATE(ObjectPattern, Expression)
  std::vector<Node*>
      properties;  // Property (value is a pattern) or RestElement
};

struct ArrayPattern : Expression {
  NODE_BOILERPLATE(ArrayPattern, Expression)
  std::vector<Expression*> elements;  // nullptr entries are holes
};

struct RestElement : Expression {
  NODE_BOILERPLATE(RestElement, Expression)
  Expression* argument = nullptr;
};

struct AssignmentPattern : Expression {
  NODE_BOILERPLATE(AssignmentPattern, Expression)
  Expression* left = nullptr;
  Expression* right = nullptr;
};

// `%Name(args)` with --allow-natives-syntax.
struct NativeCallExpression : Expression {
  NODE_BOILERPLATE(NativeCallExpression, Expression)
  std::string_view name;  // without the '%'
  std::vector<Expression*> arguments;
};

// --- Classes ---------------------------------------------------------------

struct MethodDefinition : Node {
  NODE_BOILERPLATE(MethodDefinition, Node)
  Expression* key = nullptr;
  FunctionExpression* value = nullptr;
  MethodKind method_kind = MethodKind::kMethod;
  bool computed = false;
  bool is_static = false;
};

struct PropertyDefinition : Node {
  NODE_BOILERPLATE(PropertyDefinition, Node)
  Expression* key = nullptr;
  Expression* value = nullptr;  // nullable
  bool computed = false;
  bool is_static = false;
  // `accessor x = 1;` auto-accessor (decorators proposal, --js-decorators).
  bool is_auto_accessor = false;
};

struct StaticBlock : Node {
  NODE_BOILERPLATE(StaticBlock, Node)
  std::vector<Statement*> body;
};

struct ClassBody : Node {
  NODE_BOILERPLATE(ClassBody, Node)
  std::vector<Node*>
      members;  // MethodDefinition/PropertyDefinition/StaticBlock
};

#undef NODE_BOILERPLATE

// ---------------------------------------------------------------------------
// Helpers.

// Returns the FunctionData of a function-like node, or nullptr.
FunctionData* AsFunctionData(Node* node);
const FunctionData* AsFunctionData(const Node* node);
ClassData* AsClassData(Node* node);
const ClassData* AsClassData(const Node* node);

// Calls `f(Node**)` for every child slot of `node`, in source order. Slots may
// hold nullptr (optional children, array holes); they are still visited so
// that callers can fill them. The callback may replace the slot's content.
template <typename F>
void ForEachChildSlot(Node* node, F&& f);

// Calls `f(Node*)` for every non-null child, in source order.
template <typename F>
void ForEachChild(Node* node, F&& f) {
  ForEachChildSlot(node, [&](Node** slot) {
    if (*slot != nullptr) {
      f(*slot);
    }
  });
}

// Pre-order traversal of the whole subtree; `f(Node*)` returns false to skip
// the node's children.
template <typename F>
void Walk(Node* node, F&& f) {
  if (node == nullptr) {
    return;
  }
  if (!f(node)) {
    return;
  }
  ForEachChild(node, [&](Node* child) { Walk(child, f); });
}

size_t CountNodes(const Node* node);

}  // namespace fast
}  // namespace redline

#include "experimental/redline/fast/ast_visit.h"

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_AST_H_
