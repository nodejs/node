// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_BASE_H_
#define V8_PARSING_PARSER_BASE_H_

#include <stdint.h>
#include <vector>

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/bailout-reason.h"
#include "src/base/flags.h"
#include "src/base/hashmap.h"
#include "src/base/v8-fallthrough.h"
#include "src/counters.h"
#include "src/globals.h"
#include "src/log.h"
#include "src/message-template.h"
#include "src/parsing/expression-classifier.h"
#include "src/parsing/func-name-inferrer.h"
#include "src/parsing/scanner.h"
#include "src/parsing/token.h"
#include "src/pointer-with-payload.h"
#include "src/zone/zone-chunk-list.h"

namespace v8 {
namespace internal {

enum FunctionNameValidity {
  kFunctionNameIsStrictReserved,
  kSkipFunctionNameCheck,
  kFunctionNameValidityUnknown
};

enum AllowLabelledFunctionStatement {
  kAllowLabelledFunctionStatement,
  kDisallowLabelledFunctionStatement,
};

enum class ParseFunctionFlag : uint8_t {
  kIsNormal = 0,
  kIsGenerator = 1 << 0,
  kIsAsync = 1 << 1
};

typedef base::Flags<ParseFunctionFlag> ParseFunctionFlags;

struct FormalParametersBase {
  explicit FormalParametersBase(DeclarationScope* scope) : scope(scope) {}

  int num_parameters() const {
    // Don't include the rest parameter into the function's formal parameter
    // count (esp. the SharedFunctionInfo::internal_formal_parameter_count,
    // which says whether we need to create an arguments adaptor frame).
    return arity - has_rest;
  }

  void UpdateArityAndFunctionLength(bool is_optional, bool is_rest) {
    if (!is_optional && !is_rest && function_length == arity) {
      ++function_length;
    }
    ++arity;
  }

  DeclarationScope* scope;
  bool has_rest = false;
  bool is_simple = true;
  int function_length = 0;
  int arity = 0;
};

// Stack-allocated scope to collect source ranges from the parser.
class SourceRangeScope final {
 public:
  SourceRangeScope(const Scanner* scanner, SourceRange* range)
      : scanner_(scanner), range_(range) {
    range_->start = scanner->peek_location().beg_pos;
    DCHECK_NE(range_->start, kNoSourcePosition);
    DCHECK_EQ(range_->end, kNoSourcePosition);
  }

  ~SourceRangeScope() {
    DCHECK_EQ(kNoSourcePosition, range_->end);
    range_->end = scanner_->location().end_pos;
    DCHECK_NE(range_->end, kNoSourcePosition);
  }

 private:
  const Scanner* scanner_;
  SourceRange* range_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SourceRangeScope);
};

// ----------------------------------------------------------------------------
// The RETURN_IF_PARSE_ERROR macro is a convenient macro to enforce error
// handling for functions that may fail (by returning if there was an parser
// error scanner()->has_parser_error_set).
//
// Usage:
//     foo = ParseFoo(); // may fail
//     RETURN_IF_PARSE_ERROR
//
//     SAFE_USE(foo);

#define RETURN_IF_PARSE_ERROR_CUSTOM(x, ...) \
  if (has_error()) {                         \
    return impl()->x(__VA_ARGS__);           \
  }

// Used in functions where the return type is ExpressionT.
#define RETURN_IF_PARSE_ERROR RETURN_IF_PARSE_ERROR_CUSTOM(NullExpression)

#define RETURN_IF_PARSE_ERROR_VOID \
  if (has_error()) return;

// Common base class template shared between parser and pre-parser.
// The Impl parameter is the actual class of the parser/pre-parser,
// following the Curiously Recurring Template Pattern (CRTP).
// The structure of the parser objects is roughly the following:
//
//   // A structure template containing type definitions, needed to
//   // avoid a cyclic dependency.
//   template <typename Impl>
//   struct ParserTypes;
//
//   // The parser base object, which should just implement pure
//   // parser behavior.  The Impl parameter is the actual derived
//   // class (according to CRTP), which implements impure parser
//   // behavior.
//   template <typename Impl>
//   class ParserBase { ... };
//
//   // And then, for each parser variant (e.g., parser, preparser, etc):
//   class Parser;
//
//   template <>
//   class ParserTypes<Parser> { ... };
//
//   class Parser : public ParserBase<Parser> { ... };
//
// The parser base object implements pure parsing, according to the
// language grammar.  Different parser implementations may exhibit
// different parser-driven behavior that is not considered as pure
// parsing, e.g., early error detection and reporting, AST generation, etc.

// The ParserTypes structure encapsulates the differences in the
// types used in parsing methods.  E.g., Parser methods use Expression*
// and PreParser methods use PreParserExpression.  For any given parser
// implementation class Impl, it is expected to contain the following typedefs:
//
// template <>
// struct ParserTypes<Impl> {
//   // Synonyms for ParserBase<Impl> and Impl, respectively.
//   typedef Base;
//   typedef Impl;
//   // Return types for traversing functions.
//   typedef Identifier;
//   typedef Expression;
//   typedef FunctionLiteral;
//   typedef ObjectLiteralProperty;
//   typedef ClassLiteralProperty;
//   typedef ExpressionList;
//   typedef ObjectPropertyList;
//   typedef ClassPropertyList;
//   typedef FormalParameters;
//   typedef Statement;
//   typedef StatementList;
//   typedef Block;
//   typedef BreakableStatement;
//   typedef ForStatement;
//   typedef IterationStatement;
//   // For constructing objects returned by the traversing functions.
//   typedef Factory;
//   // For other implementation-specific tasks.
//   typedef Target;
//   typedef TargetScope;
// };

template <typename Impl>
struct ParserTypes;

enum class ParsePropertyKind : uint8_t {
  kAccessorGetter,
  kAccessorSetter,
  kValue,
  kShorthand,
  kMethod,
  kClassField,
  kSpread,
  kNotSet
};

template <typename Impl>
class ParserBase {
 public:
  // Shorten type names defined by ParserTypes<Impl>.
  typedef ParserTypes<Impl> Types;
  typedef typename Types::Identifier IdentifierT;
  typedef typename Types::Expression ExpressionT;
  typedef typename Types::FunctionLiteral FunctionLiteralT;
  typedef typename Types::ObjectLiteralProperty ObjectLiteralPropertyT;
  typedef typename Types::ClassLiteralProperty ClassLiteralPropertyT;
  typedef typename Types::Suspend SuspendExpressionT;
  typedef typename Types::RewritableExpression RewritableExpressionT;
  typedef typename Types::ObjectPropertyList ObjectPropertyListT;
  typedef typename Types::FormalParameters FormalParametersT;
  typedef typename Types::Statement StatementT;
  typedef typename Types::StatementList StatementListT;
  typedef typename Types::ScopedStatementList ScopedStatementListT;
  typedef typename Types::ExpressionList ExpressionListT;
  typedef typename Types::Block BlockT;
  typedef typename Types::ForStatement ForStatementT;
  typedef typename v8::internal::ExpressionClassifier<Types>
      ExpressionClassifier;
  typedef typename Types::FuncNameInferrer FuncNameInferrer;
  typedef typename Types::FuncNameInferrer::State FuncNameInferrerState;
  typedef typename Types::SourceRange SourceRange;
  typedef typename Types::SourceRangeScope SourceRangeScope;

  // All implementation-specific methods must be called through this.
  Impl* impl() { return static_cast<Impl*>(this); }
  const Impl* impl() const { return static_cast<const Impl*>(this); }

  ParserBase(Zone* zone, Scanner* scanner, uintptr_t stack_limit,
             v8::Extension* extension, AstValueFactory* ast_value_factory,
             PendingCompilationErrorHandler* pending_error_handler,
             RuntimeCallStats* runtime_call_stats, Logger* logger,
             int script_id, bool parsing_module, bool parsing_on_main_thread)
      : scope_(nullptr),
        original_scope_(nullptr),
        function_state_(nullptr),
        extension_(extension),
        fni_(ast_value_factory),
        ast_value_factory_(ast_value_factory),
        ast_node_factory_(ast_value_factory, zone),
        runtime_call_stats_(runtime_call_stats),
        logger_(logger),
        parsing_on_main_thread_(parsing_on_main_thread),
        parsing_module_(parsing_module),
        stack_limit_(stack_limit),
        pending_error_handler_(pending_error_handler),
        zone_(zone),
        classifier_(nullptr),
        scanner_(scanner),
        default_eager_compile_hint_(FunctionLiteral::kShouldLazyCompile),
        function_literal_id_(0),
        script_id_(script_id),
        allow_natives_(false),
        allow_harmony_do_expressions_(false),
        allow_harmony_public_fields_(false),
        allow_harmony_static_fields_(false),
        allow_harmony_dynamic_import_(false),
        allow_harmony_import_meta_(false),
        allow_harmony_private_fields_(false),
        allow_eval_cache_(true) {
    pointer_buffer_.reserve(128);
  }

#define ALLOW_ACCESSORS(name)                           \
  bool allow_##name() const { return allow_##name##_; } \
  void set_allow_##name(bool allow) { allow_##name##_ = allow; }

  ALLOW_ACCESSORS(natives);
  ALLOW_ACCESSORS(harmony_do_expressions);
  ALLOW_ACCESSORS(harmony_public_fields);
  ALLOW_ACCESSORS(harmony_static_fields);
  ALLOW_ACCESSORS(harmony_dynamic_import);
  ALLOW_ACCESSORS(harmony_import_meta);
  ALLOW_ACCESSORS(eval_cache);

#undef ALLOW_ACCESSORS

  bool has_error() const { return scanner()->has_parser_error_set(); }
  bool allow_harmony_numeric_separator() const {
    return scanner()->allow_harmony_numeric_separator();
  }
  void set_allow_harmony_numeric_separator(bool allow) {
    scanner()->set_allow_harmony_numeric_separator(allow);
  }

  bool allow_harmony_private_fields() const {
    return scanner()->allow_harmony_private_fields();
  }
  void set_allow_harmony_private_fields(bool allow) {
    scanner()->set_allow_harmony_private_fields(allow);
  }

  uintptr_t stack_limit() const { return stack_limit_; }

  void set_stack_limit(uintptr_t stack_limit) { stack_limit_ = stack_limit; }

  void set_default_eager_compile_hint(
      FunctionLiteral::EagerCompileHint eager_compile_hint) {
    default_eager_compile_hint_ = eager_compile_hint;
  }

  FunctionLiteral::EagerCompileHint default_eager_compile_hint() const {
    return default_eager_compile_hint_;
  }

  int GetNextFunctionLiteralId() { return ++function_literal_id_; }
  int GetLastFunctionLiteralId() const { return function_literal_id_; }

  void SkipFunctionLiterals(int delta) { function_literal_id_ += delta; }

  void ResetFunctionLiteralId() { function_literal_id_ = 0; }

  // The Zone where the parsing outputs are stored.
  Zone* main_zone() const { return ast_value_factory()->zone(); }

  // The current Zone, which might be the main zone or a temporary Zone.
  Zone* zone() const { return zone_; }

 protected:
  friend class v8::internal::ExpressionClassifier<ParserTypes<Impl>>;

  enum AllowRestrictedIdentifiers {
    kAllowRestrictedIdentifiers,
    kDontAllowRestrictedIdentifiers
  };

  enum LazyParsingResult { kLazyParsingComplete, kLazyParsingAborted };

  enum VariableDeclarationContext {
    kStatementListItem,
    kStatement,
    kForStatement
  };

  class ClassLiteralChecker;
  class ObjectLiteralChecker;

  // ---------------------------------------------------------------------------
  // BlockState and FunctionState implement the parser's scope stack.
  // The parser's current scope is in scope_. BlockState and FunctionState
  // constructors push on the scope stack and the destructors pop. They are also
  // used to hold the parser's per-funcion state.
  class BlockState {
   public:
    BlockState(Scope** scope_stack, Scope* scope)
        : scope_stack_(scope_stack), outer_scope_(*scope_stack) {
      *scope_stack_ = scope;
    }

    BlockState(Zone* zone, Scope** scope_stack)
        : BlockState(scope_stack,
                     new (zone) Scope(zone, *scope_stack, BLOCK_SCOPE)) {}

    ~BlockState() { *scope_stack_ = outer_scope_; }

   private:
    Scope** const scope_stack_;
    Scope* const outer_scope_;
  };

  class FunctionState final : public BlockState {
   public:
    FunctionState(FunctionState** function_state_stack, Scope** scope_stack,
                  DeclarationScope* scope);
    ~FunctionState();

    DeclarationScope* scope() const { return scope_->AsDeclarationScope(); }

    void AddProperty() { expected_property_count_++; }
    int expected_property_count() { return expected_property_count_; }

    void DisableOptimization(BailoutReason reason) {
      dont_optimize_reason_ = reason;
    }
    BailoutReason dont_optimize_reason() { return dont_optimize_reason_; }

    void AddSuspend() { suspend_count_++; }
    int suspend_count() const { return suspend_count_; }
    bool CanSuspend() const { return suspend_count_ > 0; }

    FunctionKind kind() const { return scope()->function_kind(); }

    void RewindDestructuringAssignments(int pos) {
      destructuring_assignments_to_rewrite_.Rewind(pos);
    }

    void AdoptDestructuringAssignmentsFromParentState(int pos) {
      const auto& outer_assignments =
          outer_function_state_->destructuring_assignments_to_rewrite_;
      DCHECK_GE(outer_assignments.size(), pos);
      auto it = outer_assignments.begin();
      it.Advance(pos);
      for (; it != outer_assignments.end(); ++it) {
        auto expr = *it;
        expr->set_scope(scope_);
        destructuring_assignments_to_rewrite_.push_back(expr);
      }
      outer_function_state_->RewindDestructuringAssignments(pos);
    }

    const ZoneChunkList<RewritableExpressionT>&
    destructuring_assignments_to_rewrite() const {
      return destructuring_assignments_to_rewrite_;
    }

    ZoneList<typename ExpressionClassifier::Error>* GetReportedErrorList() {
      return &reported_errors_;
    }

    bool next_function_is_likely_called() const {
      return next_function_is_likely_called_;
    }

    bool previous_function_was_likely_called() const {
      return previous_function_was_likely_called_;
    }

    void set_next_function_is_likely_called() {
      next_function_is_likely_called_ = true;
    }

    void RecordFunctionOrEvalCall() { contains_function_or_eval_ = true; }
    bool contains_function_or_eval() const {
      return contains_function_or_eval_;
    }

    class FunctionOrEvalRecordingScope {
     public:
      explicit FunctionOrEvalRecordingScope(FunctionState* state)
          : state_and_prev_value_(state, state->contains_function_or_eval_) {
        state->contains_function_or_eval_ = false;
      }
      ~FunctionOrEvalRecordingScope() {
        bool found = state_and_prev_value_->contains_function_or_eval_;
        if (!found) {
          state_and_prev_value_->contains_function_or_eval_ =
              state_and_prev_value_.GetPayload();
        }
      }

     private:
      PointerWithPayload<FunctionState, bool, 1> state_and_prev_value_;
    };

   private:
    void AddDestructuringAssignment(RewritableExpressionT expr) {
      destructuring_assignments_to_rewrite_.push_back(expr);
    }

    // Properties count estimation.
    int expected_property_count_;

    // How many suspends are needed for this function.
    int suspend_count_;

    FunctionState** function_state_stack_;
    FunctionState* outer_function_state_;
    DeclarationScope* scope_;

    ZoneChunkList<RewritableExpressionT> destructuring_assignments_to_rewrite_;

    ZoneList<typename ExpressionClassifier::Error> reported_errors_;

    // A reason, if any, why this function should not be optimized.
    BailoutReason dont_optimize_reason_;

    // Record whether the next (=== immediately following) function literal is
    // preceded by a parenthesis / exclamation mark. Also record the previous
    // state.
    // These are managed by the FunctionState constructor; the caller may only
    // call set_next_function_is_likely_called.
    bool next_function_is_likely_called_;
    bool previous_function_was_likely_called_;

    // Track if a function or eval occurs within this FunctionState
    bool contains_function_or_eval_;

    friend Impl;
  };

  struct DeclarationDescriptor {
    enum Kind { NORMAL, PARAMETER, FOR_EACH };
    Scope* scope;
    VariableMode mode;
    int declaration_pos;
    int initialization_pos;
    Kind declaration_kind;
  };

  struct DeclarationParsingResult {
    struct Declaration {
      Declaration(ExpressionT pattern, int initializer_position,
                  ExpressionT initializer)
          : pattern(pattern),
            initializer_position(initializer_position),
            initializer(initializer) {}

      ExpressionT pattern;
      int initializer_position;
      int value_beg_position = kNoSourcePosition;
      ExpressionT initializer;
    };

    DeclarationParsingResult()
        : first_initializer_loc(Scanner::Location::invalid()),
          bindings_loc(Scanner::Location::invalid()) {}

    DeclarationDescriptor descriptor;
    std::vector<Declaration> declarations;
    Scanner::Location first_initializer_loc;
    Scanner::Location bindings_loc;
  };

  struct CatchInfo {
   public:
    explicit CatchInfo(ParserBase* parser)
        : name(parser->impl()->NullIdentifier()),
          pattern(parser->impl()->NullExpression()),
          scope(nullptr),
          init_block(parser->impl()->NullStatement()),
          inner_block(parser->impl()->NullStatement()),
          bound_names(1, parser->zone()) {}
    IdentifierT name;
    ExpressionT pattern;
    Scope* scope;
    BlockT init_block;
    BlockT inner_block;
    ZonePtrList<const AstRawString> bound_names;
  };

  struct ForInfo {
   public:
    explicit ForInfo(ParserBase* parser)
        : bound_names(1, parser->zone()),
          mode(ForEachStatement::ENUMERATE),
          position(kNoSourcePosition),
          parsing_result() {}
    ZonePtrList<const AstRawString> bound_names;
    ForEachStatement::VisitMode mode;
    int position;
    DeclarationParsingResult parsing_result;
  };

  struct ClassInfo {
   public:
    explicit ClassInfo(ParserBase* parser)
        : variable(nullptr),
          extends(parser->impl()->NullExpression()),
          properties(parser->impl()->NewClassPropertyList(4)),
          static_fields(parser->impl()->NewClassPropertyList(4)),
          instance_fields(parser->impl()->NewClassPropertyList(4)),
          constructor(parser->impl()->NullExpression()),
          has_seen_constructor(false),
          has_name_static_property(false),
          has_static_computed_names(false),
          has_static_class_fields(false),
          has_instance_class_fields(false),
          is_anonymous(false),
          static_fields_scope(nullptr),
          instance_fields_scope(nullptr),
          computed_field_count(0) {}
    Variable* variable;
    ExpressionT extends;
    typename Types::ClassPropertyList properties;
    typename Types::ClassPropertyList static_fields;
    typename Types::ClassPropertyList instance_fields;
    FunctionLiteralT constructor;

    bool has_seen_constructor;
    bool has_name_static_property;
    bool has_static_computed_names;
    bool has_static_class_fields;
    bool has_instance_class_fields;
    bool is_anonymous;
    DeclarationScope* static_fields_scope;
    DeclarationScope* instance_fields_scope;
    int computed_field_count;
  };

  const AstRawString* ClassFieldVariableName(AstValueFactory* ast_value_factory,
                                             int index) {
    std::string name = ".class-field-" + std::to_string(index);
    return ast_value_factory->GetOneByteString(name.c_str());
  }

  DeclarationScope* NewScriptScope() const {
    return new (zone()) DeclarationScope(zone(), ast_value_factory());
  }

  DeclarationScope* NewVarblockScope() const {
    return new (zone()) DeclarationScope(zone(), scope(), BLOCK_SCOPE);
  }

  ModuleScope* NewModuleScope(DeclarationScope* parent) const {
    return new (zone()) ModuleScope(parent, ast_value_factory());
  }

  DeclarationScope* NewEvalScope(Scope* parent) const {
    return new (zone()) DeclarationScope(zone(), parent, EVAL_SCOPE);
  }

  Scope* NewScope(ScopeType scope_type) const {
    return NewScopeWithParent(scope(), scope_type);
  }

  // This constructor should only be used when absolutely necessary. Most scopes
  // should automatically use scope() as parent, and be fine with
  // NewScope(ScopeType) above.
  Scope* NewScopeWithParent(Scope* parent, ScopeType scope_type) const {
    // Must always use the specific constructors for the blacklisted scope
    // types.
    DCHECK_NE(FUNCTION_SCOPE, scope_type);
    DCHECK_NE(SCRIPT_SCOPE, scope_type);
    DCHECK_NE(MODULE_SCOPE, scope_type);
    DCHECK_NOT_NULL(parent);
    return new (zone()) Scope(zone(), parent, scope_type);
  }

  // Creates a function scope that always allocates in zone(). The function
  // scope itself is either allocated in zone() or in target_zone if one is
  // passed in.
  DeclarationScope* NewFunctionScope(FunctionKind kind,
                                     Zone* parse_zone = nullptr) const {
    DCHECK(ast_value_factory());
    if (parse_zone == nullptr) parse_zone = zone();
    DeclarationScope* result = new (zone())
        DeclarationScope(parse_zone, scope(), FUNCTION_SCOPE, kind);

    // Record presence of an inner function scope
    function_state_->RecordFunctionOrEvalCall();

    // TODO(verwaest): Move into the DeclarationScope constructor.
    if (!IsArrowFunction(kind)) {
      result->DeclareDefaultFunctionVariables(ast_value_factory());
    }
    return result;
  }

  V8_INLINE DeclarationScope* GetDeclarationScope() const {
    return scope()->GetDeclarationScope();
  }
  V8_INLINE DeclarationScope* GetClosureScope() const {
    return scope()->GetClosureScope();
  }

  Scanner* scanner() const { return scanner_; }
  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }
  int position() const { return scanner_->location().beg_pos; }
  int peek_position() const { return scanner_->peek_location().beg_pos; }
  int end_position() const { return scanner_->location().end_pos; }
  int peek_end_position() const { return scanner_->peek_location().end_pos; }
  bool stack_overflow() const {
    return pending_error_handler()->stack_overflow();
  }
  void set_stack_overflow() {
    scanner_->set_parser_error();
    pending_error_handler()->set_stack_overflow();
  }
  void CheckStackOverflow() {
    // Any further calls to Next or peek will return the illegal token.
    if (GetCurrentStackPosition() < stack_limit_) set_stack_overflow();
  }
  int script_id() { return script_id_; }
  void set_script_id(int id) { script_id_ = id; }

  V8_INLINE Token::Value peek() { return scanner()->peek(); }

  // Returns the position past the following semicolon (if it exists), and the
  // position past the end of the current token otherwise.
  int PositionAfterSemicolon() {
    return (peek() == Token::SEMICOLON) ? peek_end_position() : end_position();
  }

  V8_INLINE Token::Value PeekAhead() { return scanner()->PeekAhead(); }

  V8_INLINE Token::Value Next() { return scanner()->Next(); }

  V8_INLINE void Consume(Token::Value token) {
    Token::Value next = scanner()->Next();
    USE(next);
    USE(token);
    DCHECK_IMPLIES(!has_error(), next == token);
  }

  V8_INLINE bool Check(Token::Value token) {
    Token::Value next = scanner()->peek();
    if (next == token) {
      Consume(next);
      return true;
    }
    return false;
  }

  void Expect(Token::Value token) {
    Token::Value next = Next();
    if (next != token) {
      ReportUnexpectedToken(next);
    }
  }

  void ExpectSemicolon() {
    // Check for automatic semicolon insertion according to
    // the rules given in ECMA-262, section 7.9, page 21.
    Token::Value tok = peek();
    if (tok == Token::SEMICOLON) {
      Next();
      return;
    }
    if (scanner()->HasLineTerminatorBeforeNext() ||
        Token::IsAutoSemicolon(tok)) {
      return;
    }

    if (scanner()->current_token() == Token::AWAIT && !is_async_function()) {
      ReportMessageAt(scanner()->location(),
                      MessageTemplate::kAwaitNotInAsyncFunction, kSyntaxError);
      return;
    }

    ReportUnexpectedToken(Next());
  }

  // Dummy functions, just useful as arguments to RETURN_IF_PARSE_ERROR_CUSTOM.
  static void Void() {}
  template <typename T>
  static T Return(T result) {
    return result;
  }

  bool peek_any_identifier() { return Token::IsAnyIdentifier(peek()); }

  bool CheckContextualKeyword(Token::Value token) {
    if (PeekContextualKeyword(token)) {
      Consume(Token::IDENTIFIER);
      return true;
    }
    return false;
  }

  bool PeekContextualKeyword(Token::Value token) {
    DCHECK(Token::IsContextualKeyword(token));
    return peek() == Token::IDENTIFIER &&
           scanner()->next_contextual_token() == token;
  }

  void ExpectMetaProperty(Token::Value property_name, const char* full_name,
                          int pos);

  void ExpectContextualKeyword(Token::Value token) {
    DCHECK(Token::IsContextualKeyword(token));
    Expect(Token::IDENTIFIER);
    if (scanner()->current_contextual_token() != token) {
      ReportUnexpectedToken(scanner()->current_token());
    }
  }

  bool CheckInOrOf(ForEachStatement::VisitMode* visit_mode) {
    if (Check(Token::IN)) {
      *visit_mode = ForEachStatement::ENUMERATE;
      return true;
    } else if (CheckContextualKeyword(Token::OF)) {
      *visit_mode = ForEachStatement::ITERATE;
      return true;
    }
    return false;
  }

  bool PeekInOrOf() {
    return peek() == Token::IN || PeekContextualKeyword(Token::OF);
  }

  // Checks whether an octal literal was last seen between beg_pos and end_pos.
  // Only called for strict mode strings.
  void CheckStrictOctalLiteral(int beg_pos, int end_pos) {
    Scanner::Location octal = scanner()->octal_position();
    if (octal.IsValid() && beg_pos <= octal.beg_pos &&
        octal.end_pos <= end_pos) {
      MessageTemplate message = scanner()->octal_message();
      DCHECK_NE(message, MessageTemplate::kNone);
      impl()->ReportMessageAt(octal, message);
      scanner()->clear_octal_position();
      if (message == MessageTemplate::kStrictDecimalWithLeadingZero) {
        impl()->CountUsage(v8::Isolate::kDecimalWithLeadingZeroInStrictMode);
      }
    }
  }

  // Checks if an octal literal or an invalid hex or unicode escape sequence
  // appears in the current template literal token. In the presence of such,
  // either returns false or reports an error, depending on should_throw.
  // Otherwise returns true.
  inline bool CheckTemplateEscapes(bool should_throw) {
    DCHECK(scanner()->current_token() == Token::TEMPLATE_SPAN ||
           scanner()->current_token() == Token::TEMPLATE_TAIL);
    if (!scanner()->has_invalid_template_escape()) {
      return true;
    }

    // Handle error case(s)
    if (should_throw) {
      impl()->ReportMessageAt(scanner()->invalid_template_escape_location(),
                              scanner()->invalid_template_escape_message());
    }
    return false;
  }

  void CheckDestructuringElement(ExpressionT element, int beg_pos, int end_pos);

  // Checking the name of a function literal. This has to be done after parsing
  // the function, since the function can declare itself strict.
  void CheckFunctionName(LanguageMode language_mode, IdentifierT function_name,
                         FunctionNameValidity function_name_validity,
                         const Scanner::Location& function_name_loc) {
    if (impl()->IsNull(function_name)) return;
    if (function_name_validity == kSkipFunctionNameCheck) return;
    // The function name needs to be checked in strict mode.
    if (is_sloppy(language_mode)) return;

    if (impl()->IsEvalOrArguments(function_name)) {
      impl()->ReportMessageAt(function_name_loc,
                              MessageTemplate::kStrictEvalArguments);
      return;
    }
    if (function_name_validity == kFunctionNameIsStrictReserved) {
      impl()->ReportMessageAt(function_name_loc,
                              MessageTemplate::kUnexpectedStrictReserved);
      return;
    }
  }

  typename Types::Factory* factory() { return &ast_node_factory_; }

  DeclarationScope* GetReceiverScope() const {
    return scope()->GetReceiverScope();
  }
  LanguageMode language_mode() { return scope()->language_mode(); }
  void RaiseLanguageMode(LanguageMode mode) {
    LanguageMode old = scope()->language_mode();
    impl()->SetLanguageMode(scope(), old > mode ? old : mode);
  }
  bool is_generator() const {
    return IsGeneratorFunction(function_state_->kind());
  }
  bool is_async_function() const {
    return IsAsyncFunction(function_state_->kind());
  }
  bool is_async_generator() const {
    return IsAsyncGeneratorFunction(function_state_->kind());
  }
  bool is_resumable() const {
    return IsResumableFunction(function_state_->kind());
  }

  const PendingCompilationErrorHandler* pending_error_handler() const {
    return pending_error_handler_;
  }
  PendingCompilationErrorHandler* pending_error_handler() {
    return pending_error_handler_;
  }

  // Report syntax errors.
  void ReportMessage(MessageTemplate message) {
    Scanner::Location source_location = scanner()->location();
    impl()->ReportMessageAt(source_location, message,
                            static_cast<const char*>(nullptr), kSyntaxError);
  }

  template <typename T>
  void ReportMessage(MessageTemplate message, T arg,
                     ParseErrorType error_type = kSyntaxError) {
    Scanner::Location source_location = scanner()->location();
    impl()->ReportMessageAt(source_location, message, arg, error_type);
  }

  void ReportMessageAt(Scanner::Location location, MessageTemplate message,
                       ParseErrorType error_type) {
    impl()->ReportMessageAt(location, message,
                            static_cast<const char*>(nullptr), error_type);
  }

  void ReportUnexpectedToken(Token::Value token);
  void ReportUnexpectedTokenAt(
      Scanner::Location location, Token::Value token,
      MessageTemplate message = MessageTemplate::kUnexpectedToken);

  void ReportClassifierError(
      const typename ExpressionClassifier::Error& error) {
    if (classifier()->does_error_reporting()) {
      impl()->ReportMessageAt(error.location, error.message(), error.arg);
    } else {
      impl()->ReportUnidentifiableError();
    }
  }

  void ValidateExpression() {
    if (!classifier()->is_valid_expression()) {
      ReportClassifierError(classifier()->expression_error());
    }
  }

  void ValidateFormalParameterInitializer() {
    if (!classifier()->is_valid_formal_parameter_initializer()) {
      ReportClassifierError(classifier()->formal_parameter_initializer_error());
    }
  }

  void ValidateBindingPattern() {
    if (!classifier()->is_valid_binding_pattern()) {
      ReportClassifierError(classifier()->binding_pattern_error());
    }
  }

  void ValidateAssignmentPattern() {
    if (!classifier()->is_valid_assignment_pattern()) {
      ReportClassifierError(classifier()->assignment_pattern_error());
    }
  }

  void ValidateFormalParameters(LanguageMode language_mode,
                                bool allow_duplicates) {
    if (!allow_duplicates &&
        !classifier()->is_valid_formal_parameter_list_without_duplicates()) {
      ReportClassifierError(classifier()->duplicate_formal_parameter_error());
    } else if (is_strict(language_mode) &&
               !classifier()->is_valid_strict_mode_formal_parameters()) {
      ReportClassifierError(classifier()->strict_mode_formal_parameter_error());
    }
  }

  void ValidateArrowFormalParameters(ExpressionT expr,
                                     bool parenthesized_formals,
                                     bool is_async) {
    if (!parenthesized_formals) {
      // A simple arrow formal parameter: async? IDENTIFIER => BODY.
      if (!impl()->IsIdentifier(expr)) {
        if (classifier()->is_valid_binding_pattern()) {
          // Non-parenthesized destructuring param.
          impl()->ReportMessageAt(
              Scanner::Location(expr->position(), position()),
              MessageTemplate::kMalformedArrowFunParamList);
        } else {
          // Otherwise simply throw where we detect that it's not a valid
          // binding pattern.
          ReportClassifierError(classifier()->binding_pattern_error());
        }
      }
    } else if (!classifier()->is_valid_arrow_formal_parameters()) {
      ReportClassifierError(classifier()->arrow_formal_parameters_error());
    } else {
      DCHECK_IMPLIES(is_async,
                     classifier()->is_valid_async_arrow_formal_parameters());
    }
  }

  void ValidateLetPattern() {
    if (!classifier()->is_valid_let_pattern()) {
      ReportClassifierError(classifier()->let_pattern_error());
    }
  }

  void BindingPatternUnexpectedToken() {
    MessageTemplate message = MessageTemplate::kUnexpectedToken;
    const char* arg = nullptr;
    Scanner::Location location = scanner()->peek_location();
    impl()->GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier()->RecordBindingPatternError(location, message, arg);
  }

  void ArrowFormalParametersUnexpectedToken() {
    MessageTemplate message = MessageTemplate::kUnexpectedToken;
    const char* arg = nullptr;
    Scanner::Location location = scanner()->peek_location();
    impl()->GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier()->RecordArrowFormalParametersError(location, message, arg);
  }

  // Parses an identifier that is valid for the current scope, in particular it
  // fails on strict mode future reserved keywords in a strict scope. If
  // allow_eval_or_arguments is kAllowEvalOrArguments, we allow "eval" or
  // "arguments" as identifier even in strict mode (this is needed in cases like
  // "var foo = eval;").
  IdentifierT ParseIdentifier(AllowRestrictedIdentifiers);
  IdentifierT ParseAndClassifyIdentifier();
  // Parses an identifier or a strict mode future reserved word, and indicate
  // whether it is strict mode future reserved. Allows passing in function_kind
  // for the case of parsing the identifier in a function expression, where the
  // relevant "function_kind" bit is of the function being parsed, not the
  // containing function.
  IdentifierT ParseIdentifierOrStrictReservedWord(FunctionKind function_kind,
                                                  bool* is_strict_reserved,
                                                  bool* is_await);
  IdentifierT ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                  bool* is_await) {
    return ParseIdentifierOrStrictReservedWord(function_state_->kind(),
                                               is_strict_reserved, is_await);
  }

  V8_INLINE IdentifierT ParseIdentifierName();

  ExpressionT ParseIdentifierNameOrPrivateName();

  ExpressionT ParseRegExpLiteral();

  ExpressionT ParseBindingPattern();
  ExpressionT ParsePrimaryExpression();

  // Use when parsing an expression that is known to not be a pattern or part
  // of a pattern.
  V8_INLINE ExpressionT ParseExpression();

  // This method does not wrap the parsing of the expression inside a
  // new expression classifier; it uses the top-level classifier instead.
  // It should be used whenever we're parsing something with the "cover"
  // grammar that recognizes both patterns and non-patterns (which roughly
  // corresponds to what's inside the parentheses generated by the symbol
  // "CoverParenthesizedExpressionAndArrowParameterList" in the ES 2017
  // specification).
  ExpressionT ParseExpressionCoverGrammar(bool accept_IN);

  ExpressionT ParseArrayLiteral();

  inline static bool IsAccessor(ParsePropertyKind kind) {
    return IsInRange(kind, ParsePropertyKind::kAccessorGetter,
                     ParsePropertyKind::kAccessorSetter);
  }

  ExpressionT ParsePropertyName(IdentifierT* name, ParsePropertyKind* kind,
                                ParseFunctionFlags* flags,
                                bool* is_computed_name, bool* is_private);
  ExpressionT ParseObjectLiteral();
  ClassLiteralPropertyT ParseClassPropertyDefinition(
      ClassLiteralChecker* checker, ClassInfo* class_info,
      IdentifierT* property_name, bool has_extends, bool* is_computed_name,
      ClassLiteralProperty::Kind* property_kind, bool* is_static,
      bool* is_private);
  ExpressionT ParseClassFieldInitializer(ClassInfo* class_info, int beg_pos,
                                         bool is_static);
  ObjectLiteralPropertyT ParseObjectPropertyDefinition(
      ObjectLiteralChecker* checker, bool* is_computed_name,
      bool* is_rest_property);
  void ParseArguments(ExpressionListT* args, bool* has_spread,
                      bool maybe_arrow);
  void ParseArguments(ExpressionListT* args, bool* has_spread) {
    ParseArguments(args, has_spread, false);
  }

  ExpressionT ParseAssignmentExpression(bool accept_IN);
  ExpressionT ParseYieldExpression(bool accept_IN);
  V8_INLINE ExpressionT ParseConditionalExpression(bool accept_IN);
  ExpressionT ParseConditionalContinuation(ExpressionT expression,
                                           bool accept_IN, int pos);
  ExpressionT ParseBinaryContinuation(ExpressionT x, int prec, int prec1,
                                      bool accept_IN);
  V8_INLINE ExpressionT ParseBinaryExpression(int prec, bool accept_IN);
  ExpressionT ParseUnaryOpExpression();
  ExpressionT ParseAwaitExpression();
  ExpressionT ParsePrefixExpression();
  V8_INLINE ExpressionT ParseUnaryExpression();
  V8_INLINE ExpressionT ParsePostfixExpression();
  V8_INLINE ExpressionT ParseLeftHandSideExpression();
  ExpressionT ParseLeftHandSideContinuation(ExpressionT expression);
  ExpressionT ParseMemberWithPresentNewPrefixesExpression();
  V8_INLINE ExpressionT ParseMemberWithNewPrefixesExpression();
  ExpressionT ParseFunctionExpression();
  V8_INLINE ExpressionT ParseMemberExpression();
  V8_INLINE ExpressionT
  ParseMemberExpressionContinuation(ExpressionT expression) {
    if (!Token::IsProperty(peek())) return expression;
    return DoParseMemberExpressionContinuation(expression);
  }
  ExpressionT DoParseMemberExpressionContinuation(ExpressionT expression);

  // `rewritable_length`: length of the destructuring_assignments_to_rewrite()
  // queue in the parent function state, prior to parsing of formal parameters.
  // If the arrow function is lazy, any items added during formal parameter
  // parsing are removed from the queue.
  ExpressionT ParseArrowFunctionLiteral(bool accept_IN,
                                        const FormalParametersT& parameters,
                                        int rewritable_length);
  void ParseAsyncFunctionBody(Scope* scope, StatementListT body);
  ExpressionT ParseAsyncFunctionLiteral();
  ExpressionT ParseClassLiteral(IdentifierT name,
                                Scanner::Location class_name_location,
                                bool name_is_strict_reserved,
                                int class_token_pos);
  ExpressionT ParseTemplateLiteral(ExpressionT tag, int start, bool tagged);
  ExpressionT ParseSuperExpression(bool is_new);
  ExpressionT ParseImportExpressions();
  ExpressionT ParseNewTargetExpression();

  V8_INLINE void ParseFormalParameter(FormalParametersT* parameters);
  void ParseFormalParameterList(FormalParametersT* parameters);
  void CheckArityRestrictions(int param_count, FunctionKind function_type,
                              bool has_rest, int formals_start_pos,
                              int formals_end_pos);

  BlockT ParseVariableDeclarations(VariableDeclarationContext var_context,
                                   DeclarationParsingResult* parsing_result,
                                   ZonePtrList<const AstRawString>* names);
  StatementT ParseAsyncFunctionDeclaration(
      ZonePtrList<const AstRawString>* names, bool default_export);
  StatementT ParseFunctionDeclaration();
  StatementT ParseHoistableDeclaration(ZonePtrList<const AstRawString>* names,
                                       bool default_export);
  StatementT ParseHoistableDeclaration(int pos, ParseFunctionFlags flags,
                                       ZonePtrList<const AstRawString>* names,
                                       bool default_export);
  StatementT ParseClassDeclaration(ZonePtrList<const AstRawString>* names,
                                   bool default_export);
  StatementT ParseNativeDeclaration();

  // Whether we're parsing a single-expression arrow function or something else.
  enum class FunctionBodyType { kExpression, kBlock };
  // Consumes the ending }.
  void ParseFunctionBody(StatementListT result, IdentifierT function_name,
                         int pos, const FormalParametersT& parameters,
                         FunctionKind kind,
                         FunctionLiteral::FunctionType function_type,
                         FunctionBodyType body_type, bool accept_IN);

  // Under some circumstances, we allow preparsing to abort if the preparsed
  // function is "long and trivial", and fully parse instead. Our current
  // definition of "long and trivial" is:
  // - over kLazyParseTrialLimit statements
  // - all starting with an identifier (i.e., no if, for, while, etc.)
  static const int kLazyParseTrialLimit = 200;

  // TODO(nikolaos, marja): The first argument should not really be passed
  // by value. The method is expected to add the parsed statements to the
  // list. This works because in the case of the parser, StatementListT is
  // a pointer whereas the preparser does not really modify the body.
  V8_INLINE void ParseStatementList(StatementListT body,
                                    Token::Value end_token) {
    LazyParsingResult result = ParseStatementList(body, end_token, false);
    USE(result);
    DCHECK_EQ(result, kLazyParsingComplete);
  }
  V8_INLINE LazyParsingResult ParseStatementList(StatementListT body,
                                                 Token::Value end_token,
                                                 bool may_abort);
  StatementT ParseStatementListItem();

  StatementT ParseStatement(ZonePtrList<const AstRawString>* labels,
                            ZonePtrList<const AstRawString>* own_labels) {
    return ParseStatement(labels, own_labels,
                          kDisallowLabelledFunctionStatement);
  }
  StatementT ParseStatement(ZonePtrList<const AstRawString>* labels,
                            ZonePtrList<const AstRawString>* own_labels,
                            AllowLabelledFunctionStatement allow_function);
  BlockT ParseBlock(ZonePtrList<const AstRawString>* labels);

  // Parse a SubStatement in strict mode, or with an extra block scope in
  // sloppy mode to handle
  // ES#sec-functiondeclarations-in-ifstatement-statement-clauses
  StatementT ParseScopedStatement(ZonePtrList<const AstRawString>* labels);

  StatementT ParseVariableStatement(VariableDeclarationContext var_context,
                                    ZonePtrList<const AstRawString>* names);

  // Magical syntax support.
  ExpressionT ParseV8Intrinsic();

  ExpressionT ParseDoExpression();

  StatementT ParseDebuggerStatement();

  StatementT ParseExpressionOrLabelledStatement(
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels,
      AllowLabelledFunctionStatement allow_function);
  StatementT ParseIfStatement(ZonePtrList<const AstRawString>* labels);
  StatementT ParseContinueStatement();
  StatementT ParseBreakStatement(ZonePtrList<const AstRawString>* labels);
  StatementT ParseReturnStatement();
  StatementT ParseWithStatement(ZonePtrList<const AstRawString>* labels);
  StatementT ParseDoWhileStatement(ZonePtrList<const AstRawString>* labels,
                                   ZonePtrList<const AstRawString>* own_labels);
  StatementT ParseWhileStatement(ZonePtrList<const AstRawString>* labels,
                                 ZonePtrList<const AstRawString>* own_labels);
  StatementT ParseThrowStatement();
  StatementT ParseSwitchStatement(ZonePtrList<const AstRawString>* labels);
  V8_INLINE StatementT ParseTryStatement();
  StatementT ParseForStatement(ZonePtrList<const AstRawString>* labels,
                               ZonePtrList<const AstRawString>* own_labels);
  StatementT ParseForEachStatementWithDeclarations(
      int stmt_pos, ForInfo* for_info, ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, Scope* inner_block_scope);
  StatementT ParseForEachStatementWithoutDeclarations(
      int stmt_pos, ExpressionT expression, int lhs_beg_pos, int lhs_end_pos,
      ForInfo* for_info, ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels);

  // Parse a C-style for loop: 'for (<init>; <cond>; <next>) { ... }'
  // "for (<init>;" is assumed to have been parser already.
  ForStatementT ParseStandardForLoop(
      int stmt_pos, ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, ExpressionT* cond,
      StatementT* next, StatementT* body);
  // Same as the above, but handles those cases where <init> is a
  // lexical variable declaration.
  StatementT ParseStandardForLoopWithLexicalDeclarations(
      int stmt_pos, StatementT init, ForInfo* for_info,
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels);
  StatementT ParseForAwaitStatement(
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels);

  bool IsNextLetKeyword();

  // Checks if the expression is a valid reference expression (e.g., on the
  // left-hand side of assignments). Although ruled out by ECMA as early errors,
  // we allow calls for web compatibility and rewrite them to a runtime throw.
  V8_INLINE ExpressionT
  CheckAndRewriteReferenceExpression(ExpressionT expression, int beg_pos,
                                     int end_pos, MessageTemplate message);
  ExpressionT CheckAndRewriteReferenceExpression(ExpressionT expression,
                                                 int beg_pos, int end_pos,
                                                 MessageTemplate message,
                                                 ParseErrorType type);

  bool IsValidReferenceExpression(ExpressionT expression);

  bool IsAssignableIdentifier(ExpressionT expression) {
    if (!impl()->IsIdentifier(expression)) return false;
    if (is_strict(language_mode()) &&
        impl()->IsEvalOrArguments(impl()->AsIdentifier(expression))) {
      return false;
    }
    return true;
  }

  bool IsValidPattern(ExpressionT expression) {
    return expression->IsObjectLiteral() || expression->IsArrayLiteral();
  }

  // Due to hoisting, the value of a 'var'-declared variable may actually change
  // even if the code contains only the "initial" assignment, namely when that
  // assignment occurs inside a loop.  For example:
  //
  //   let i = 10;
  //   do { var x = i } while (i--):
  //
  // As a simple and very conservative approximation of this, we explicitly mark
  // as maybe-assigned any non-lexical variable whose initializing "declaration"
  // does not syntactically occur in the function scope.  (In the example above,
  // it occurs in a block scope.)
  //
  // Note that non-lexical variables include temporaries, which may also get
  // assigned inside a loop due to the various rewritings that the parser
  // performs.
  //
  // This also handles marking of loop variables in for-in and for-of loops,
  // as determined by declaration_kind.
  //
  static void MarkLoopVariableAsAssigned(
      Scope* scope, Variable* var,
      typename DeclarationDescriptor::Kind declaration_kind);

  FunctionKind FunctionKindForImpl(bool is_method, ParseFunctionFlags flags) {
    static const FunctionKind kFunctionKinds[][2][2] = {
        {
            // is_method=false
            {// is_generator=false
             FunctionKind::kNormalFunction, FunctionKind::kAsyncFunction},
            {// is_generator=true
             FunctionKind::kGeneratorFunction,
             FunctionKind::kAsyncGeneratorFunction},
        },
        {
            // is_method=true
            {// is_generator=false
             FunctionKind::kConciseMethod, FunctionKind::kAsyncConciseMethod},
            {// is_generator=true
             FunctionKind::kConciseGeneratorMethod,
             FunctionKind::kAsyncConciseGeneratorMethod},
        }};
    return kFunctionKinds[is_method]
                         [(flags & ParseFunctionFlag::kIsGenerator) != 0]
                         [(flags & ParseFunctionFlag::kIsAsync) != 0];
  }

  inline FunctionKind FunctionKindFor(ParseFunctionFlags flags) {
    const bool kIsMethod = false;
    return FunctionKindForImpl(kIsMethod, flags);
  }

  inline FunctionKind MethodKindFor(ParseFunctionFlags flags) {
    const bool kIsMethod = true;
    return FunctionKindForImpl(kIsMethod, flags);
  }

  // Keep track of eval() calls since they disable all local variable
  // optimizations. This checks if expression is an eval call, and if yes,
  // forwards the information to scope.
  Call::PossiblyEval CheckPossibleEvalCall(ExpressionT expression,
                                           Scope* scope) {
    if (impl()->IsIdentifier(expression) &&
        impl()->IsEval(impl()->AsIdentifier(expression))) {
      scope->RecordInnerScopeEvalCall();
      function_state_->RecordFunctionOrEvalCall();
      if (is_sloppy(scope->language_mode())) {
        // For sloppy scopes we also have to record the call at function level,
        // in case it includes declarations that will be hoisted.
        scope->GetDeclarationScope()->RecordEvalCall();
      }

      // This call is only necessary to track evals that may be
      // inside arrow function parameter lists. In that case,
      // Scope::Snapshot::Reparent will move this bit down into
      // the arrow function's scope.
      scope->RecordEvalCall();

      return Call::IS_POSSIBLY_EVAL;
    }
    return Call::NOT_EVAL;
  }

  // Convenience method which determines the type of return statement to emit
  // depending on the current function type.
  inline StatementT BuildReturnStatement(ExpressionT expr, int pos,
                                         int end_pos = kNoSourcePosition) {
    if (impl()->IsNull(expr)) {
      expr = factory()->NewUndefinedLiteral(kNoSourcePosition);
    } else if (is_async_generator()) {
      // In async generators, if there is an explicit operand to the return
      // statement, await the operand.
      expr = factory()->NewAwait(expr, kNoSourcePosition);
      function_state_->AddSuspend();
    }
    if (is_async_function()) {
      return factory()->NewAsyncReturnStatement(expr, pos, end_pos);
    }
    return factory()->NewReturnStatement(expr, pos, end_pos);
  }

  // Validation per ES6 object literals.
  class ObjectLiteralChecker {
   public:
    explicit ObjectLiteralChecker(ParserBase* parser)
        : parser_(parser), has_seen_proto_(false) {}

    void CheckDuplicateProto(Token::Value property);

   private:
    bool IsProto() const {
      return this->scanner()->CurrentMatchesContextualEscaped(
          Token::PROTO_UNDERSCORED);
    }

    ParserBase* parser() const { return parser_; }
    Scanner* scanner() const { return parser_->scanner(); }

    ParserBase* parser_;
    bool has_seen_proto_;
  };

  // Validation per ES6 class literals.
  class ClassLiteralChecker {
   public:
    explicit ClassLiteralChecker(ParserBase* parser)
        : parser_(parser), has_seen_constructor_(false) {}

    void CheckClassMethodName(Token::Value property, ParsePropertyKind type,
                              ParseFunctionFlags flags, bool is_static);
    void CheckClassFieldName(bool is_static);

   private:
    bool IsConstructor() {
      return this->scanner()->CurrentMatchesContextualEscaped(
          Token::CONSTRUCTOR);
    }
    bool IsPrivateConstructor() {
      return this->scanner()->CurrentMatchesContextualEscaped(
          Token::PRIVATE_CONSTRUCTOR);
    }
    bool IsPrototype() {
      return this->scanner()->CurrentMatchesContextualEscaped(Token::PROTOTYPE);
    }

    ParserBase* parser() const { return parser_; }
    Scanner* scanner() const { return parser_->scanner(); }

    ParserBase* parser_;
    bool has_seen_constructor_;
  };

  ModuleDescriptor* module() const {
    return scope()->AsModuleScope()->module();
  }
  Scope* scope() const { return scope_; }

  // Stack of expression classifiers.
  // The top of the stack is always pointed to by classifier().
  V8_INLINE ExpressionClassifier* classifier() const {
    DCHECK_NOT_NULL(classifier_);
    return classifier_;
  }

  // Accumulates the classifier that is on top of the stack (inner) to
  // the one that is right below (outer) and pops the inner.
  V8_INLINE void Accumulate(unsigned productions) {
    DCHECK_NOT_NULL(classifier_);
    ExpressionClassifier* previous = classifier_->previous();
    DCHECK_NOT_NULL(previous);
    previous->Accumulate(classifier_, productions);
    classifier_ = previous;
  }

  V8_INLINE void AccumulateNonBindingPatternErrors() {
    this->Accumulate(ExpressionClassifier::AllProductions &
                     ~(ExpressionClassifier::BindingPatternProduction |
                       ExpressionClassifier::LetPatternProduction));
  }

  // Accumulate errors that can be arbitrarily deep in an expression.
  // These correspond to the ECMAScript spec's 'Contains' operation
  // on productions. This includes:
  //
  // - YieldExpression is disallowed in arrow parameters in a generator.
  // - AwaitExpression is disallowed in arrow parameters in an async function.
  // - AwaitExpression is disallowed in async arrow parameters.
  //
  V8_INLINE void AccumulateFormalParameterContainmentErrors() {
    Accumulate(ExpressionClassifier::FormalParameterInitializerProduction |
               ExpressionClassifier::AsyncArrowFormalParametersProduction);
  }

  std::vector<void*>* pointer_buffer() { return &pointer_buffer_; }

  // Parser base's protected field members.

  Scope* scope_;                   // Scope stack.
  Scope* original_scope_;  // The top scope for the current parsing item.
  FunctionState* function_state_;  // Function state stack.
  v8::Extension* extension_;
  FuncNameInferrer fni_;
  AstValueFactory* ast_value_factory_;  // Not owned.
  typename Types::Factory ast_node_factory_;
  RuntimeCallStats* runtime_call_stats_;
  internal::Logger* logger_;
  bool parsing_on_main_thread_;
  const bool parsing_module_;
  uintptr_t stack_limit_;
  PendingCompilationErrorHandler* pending_error_handler_;

  // Parser base's private field members.

 private:
  Zone* zone_;
  ExpressionClassifier* classifier_;

  std::vector<void*> pointer_buffer_;

  Scanner* scanner_;

  FunctionLiteral::EagerCompileHint default_eager_compile_hint_;

  int function_literal_id_;
  int script_id_;

  bool allow_natives_;
  bool allow_harmony_do_expressions_;
  bool allow_harmony_public_fields_;
  bool allow_harmony_static_fields_;
  bool allow_harmony_dynamic_import_;
  bool allow_harmony_import_meta_;
  bool allow_harmony_private_fields_;
  bool allow_eval_cache_;
};

template <typename Impl>
ParserBase<Impl>::FunctionState::FunctionState(
    FunctionState** function_state_stack, Scope** scope_stack,
    DeclarationScope* scope)
    : BlockState(scope_stack, scope),
      expected_property_count_(0),
      suspend_count_(0),
      function_state_stack_(function_state_stack),
      outer_function_state_(*function_state_stack),
      scope_(scope),
      destructuring_assignments_to_rewrite_(scope->zone()),
      reported_errors_(16, scope->zone()),
      dont_optimize_reason_(BailoutReason::kNoReason),
      next_function_is_likely_called_(false),
      previous_function_was_likely_called_(false),
      contains_function_or_eval_(false) {
  *function_state_stack = this;
  if (outer_function_state_) {
    outer_function_state_->previous_function_was_likely_called_ =
        outer_function_state_->next_function_is_likely_called_;
    outer_function_state_->next_function_is_likely_called_ = false;
  }
}

template <typename Impl>
ParserBase<Impl>::FunctionState::~FunctionState() {
  *function_state_stack_ = outer_function_state_;
}

template <typename Impl>
void ParserBase<Impl>::ReportUnexpectedToken(Token::Value token) {
  return ReportUnexpectedTokenAt(scanner_->location(), token);
}

template <typename Impl>
void ParserBase<Impl>::ReportUnexpectedTokenAt(
    Scanner::Location source_location, Token::Value token,
    MessageTemplate message) {
  const char* arg = nullptr;
  impl()->GetUnexpectedTokenMessage(token, &message, &source_location, &arg);
  if (Impl::IsPreParser()) {
    impl()->ReportUnidentifiableError();
  } else {
    impl()->ReportMessageAt(source_location, message, arg);
  }
}

template <typename Impl>
typename ParserBase<Impl>::IdentifierT ParserBase<Impl>::ParseIdentifier(
    AllowRestrictedIdentifiers allow_restricted_identifiers) {
  ExpressionClassifier classifier(this);
  auto result = ParseAndClassifyIdentifier();

  if (allow_restricted_identifiers == kDontAllowRestrictedIdentifiers) {
    ValidateBindingPattern();
  }

  return result;
}

template <typename Impl>
typename ParserBase<Impl>::IdentifierT
ParserBase<Impl>::ParseAndClassifyIdentifier() {
  Token::Value next = Next();
  STATIC_ASSERT(Token::IDENTIFIER + 1 == Token::ASYNC);
  if (IsInRange(next, Token::IDENTIFIER, Token::ASYNC)) {
    IdentifierT name = impl()->GetSymbol();

    // When this function is used to read a formal parameter, we don't always
    // know whether the function is going to be strict or sloppy.  Indeed for
    // arrow functions we don't always know that the identifier we are reading
    // is actually a formal parameter.  Therefore besides the errors that we
    // must detect because we know we're in strict mode, we also record any
    // error that we might make in the future once we know the language mode.
    if (impl()->IsEvalOrArguments(name)) {
      if (impl()->IsArguments(name) && scope()->ShouldBanArguments()) {
        ReportMessage(MessageTemplate::kArgumentsDisallowedInInitializer);
        return impl()->NullIdentifier();
      }

      classifier()->RecordStrictModeFormalParameterError(
          scanner()->location(), MessageTemplate::kStrictEvalArguments);
      if (is_strict(language_mode())) {
        classifier()->RecordBindingPatternError(
            scanner()->location(), MessageTemplate::kStrictEvalArguments);
      }
    }

    return name;
  } else if (next == Token::AWAIT && !parsing_module_ && !is_async_function()) {
    classifier()->RecordAsyncArrowFormalParametersError(
        scanner()->location(), MessageTemplate::kAwaitBindingIdentifier);
    return impl()->GetSymbol();
  } else if (is_sloppy(language_mode()) &&
             (Token::IsStrictReservedWord(next) ||
              (next == Token::YIELD && !is_generator()))) {
    classifier()->RecordStrictModeFormalParameterError(
        scanner()->location(), MessageTemplate::kUnexpectedStrictReserved);
    if (scanner()->IsLet()) {
      classifier()->RecordLetPatternError(
          scanner()->location(), MessageTemplate::kLetInLexicalBinding);
    }
    return impl()->GetSymbol();
  } else {
    ReportUnexpectedToken(next);
    return impl()->NullIdentifier();
  }
}

template <class Impl>
typename ParserBase<Impl>::IdentifierT
ParserBase<Impl>::ParseIdentifierOrStrictReservedWord(
    FunctionKind function_kind, bool* is_strict_reserved, bool* is_await) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER || (next == Token::AWAIT && !parsing_module_ &&
                                    !IsAsyncFunction(function_kind)) ||
      next == Token::ASYNC) {
    *is_strict_reserved = false;
    *is_await = next == Token::AWAIT;
  } else if (Token::IsStrictReservedWord(next) ||
             (next == Token::YIELD && !IsGeneratorFunction(function_kind))) {
    *is_strict_reserved = true;
  } else {
    ReportUnexpectedToken(next);
    return impl()->NullIdentifier();
  }

  return impl()->GetSymbol();
}

template <typename Impl>
typename ParserBase<Impl>::IdentifierT ParserBase<Impl>::ParseIdentifierName() {
  Token::Value next = Next();
  if (!Token::IsAnyIdentifier(next) && next != Token::ESCAPED_KEYWORD &&
      !Token::IsKeyword(next)) {
    ReportUnexpectedToken(next);
    return impl()->NullIdentifier();
  }

  return impl()->GetSymbol();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseIdentifierNameOrPrivateName() {
  int pos = position();
  IdentifierT name;
  ExpressionT key;
  if (allow_harmony_private_fields() && Check(Token::PRIVATE_NAME)) {
    name = impl()->GetSymbol();
    auto key_proxy =
        impl()->ExpressionFromIdentifier(name, pos, InferName::kNo);
    key_proxy->set_is_private_field();
    key = key_proxy;
  } else {
    name = ParseIdentifierName();
    key = factory()->NewStringLiteral(name, pos);
  }
  RETURN_IF_PARSE_ERROR;
  impl()->PushLiteralName(name);
  return key;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseRegExpLiteral() {
  int pos = peek_position();
  if (!scanner()->ScanRegExpPattern()) {
    Next();
    ReportMessage(MessageTemplate::kUnterminatedRegExp);
    return impl()->NullExpression();
  }

  IdentifierT js_pattern = impl()->GetNextSymbol();
  Maybe<RegExp::Flags> flags = scanner()->ScanRegExpFlags();
  if (flags.IsNothing()) {
    Next();
    ReportMessage(MessageTemplate::kMalformedRegExpFlags);
    return impl()->NullExpression();
  }
  int js_flags = flags.FromJust();
  Next();
  return factory()->NewRegExpLiteral(js_pattern, js_flags, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseBindingPattern() {
  // Pattern ::
  //   Identifier
  //   ArrayLiteral
  //   ObjectLiteral

  int beg_pos = peek_position();
  Token::Value token = peek();
  ExpressionT result;

  if (Token::IsAnyIdentifier(token)) {
    IdentifierT name = ParseAndClassifyIdentifier();
    RETURN_IF_PARSE_ERROR;
    result = impl()->ExpressionFromIdentifier(name, beg_pos);
  } else {
    classifier()->RecordNonSimpleParameter();

    if (token == Token::LBRACK) {
      result = ParseArrayLiteral();
    } else if (token == Token::LBRACE) {
      result = ParseObjectLiteral();
    } else {
      ReportUnexpectedToken(Next());
      return impl()->NullExpression();
    }
  }

  ValidateBindingPattern();
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParsePrimaryExpression() {
  CheckStackOverflow();

  // PrimaryExpression ::
  //   'this'
  //   'null'
  //   'true'
  //   'false'
  //   Identifier
  //   Number
  //   String
  //   ArrayLiteral
  //   ObjectLiteral
  //   RegExpLiteral
  //   ClassLiteral
  //   '(' Expression ')'
  //   TemplateLiteral
  //   do Block
  //   AsyncFunctionLiteral

  int beg_pos = peek_position();
  Token::Value token = peek();

  if (IsInRange(token, Token::IDENTIFIER,
                Token::ESCAPED_STRICT_RESERVED_WORD)) {
    // Using eval or arguments in this context is OK even in strict mode.
    IdentifierT name = ParseAndClassifyIdentifier();
    InferName infer = InferName::kYes;
    if (V8_UNLIKELY(impl()->IsAsync(name) &&
                    !scanner()->HasLineTerminatorBeforeNext())) {
      if (peek() == Token::FUNCTION) {
        BindingPatternUnexpectedToken();
        return ParseAsyncFunctionLiteral();
      }
      // async Identifier => AsyncConciseBody
      if (peek_any_identifier() && PeekAhead() == Token::ARROW) {
        name = ParseAndClassifyIdentifier();

        if (!classifier()->is_valid_async_arrow_formal_parameters()) {
          ReportClassifierError(
              classifier()->async_arrow_formal_parameters_error());
          return impl()->NullExpression();
        }
        infer = InferName::kNo;
      }
    }
    RETURN_IF_PARSE_ERROR;
    return impl()->ExpressionFromIdentifier(name, beg_pos, infer);
  }
  DCHECK_IMPLIES(Token::IsAnyIdentifier(token), token == Token::ENUM);

  if (Token::IsLiteral(token)) {
    BindingPatternUnexpectedToken();
    return impl()->ExpressionFromLiteral(Next(), beg_pos);
  }

  switch (token) {
    case Token::THIS: {
      BindingPatternUnexpectedToken();
      Consume(Token::THIS);
      return impl()->ThisExpression(beg_pos);
    }

    case Token::ASSIGN_DIV:
    case Token::DIV:
      classifier()->RecordBindingPatternError(
          scanner()->peek_location(), MessageTemplate::kUnexpectedTokenRegExp);
      return ParseRegExpLiteral();

    case Token::LBRACK:
      return ParseArrayLiteral();

    case Token::LBRACE:
      return ParseObjectLiteral();

    case Token::LPAREN: {
      // Arrow function formal parameters are either a single identifier or a
      // list of BindingPattern productions enclosed in parentheses.
      // Parentheses are not valid on the LHS of a BindingPattern, so we use
      // the is_valid_binding_pattern() check to detect multiple levels of
      // parenthesization.
      bool pattern_error = !classifier()->is_valid_binding_pattern();
      classifier()->RecordPatternError(scanner()->peek_location(),
                                       MessageTemplate::kUnexpectedToken,
                                       Token::String(Token::LPAREN));
      if (pattern_error) ArrowFormalParametersUnexpectedToken();
      Consume(Token::LPAREN);
      if (Check(Token::RPAREN)) {
        // ()=>x.  The continuation that looks for the => is in
        // ParseAssignmentExpression.
        classifier()->RecordExpressionError(scanner()->location(),
                                            MessageTemplate::kUnexpectedToken,
                                            Token::String(Token::RPAREN));
        return factory()->NewEmptyParentheses(beg_pos);
      }
      // Heuristically try to detect immediately called functions before
      // seeing the call parentheses.
      if (peek() == Token::FUNCTION ||
          (peek() == Token::ASYNC && PeekAhead() == Token::FUNCTION)) {
        function_state_->set_next_function_is_likely_called();
      }
      ExpressionT expr = ParseExpressionCoverGrammar(true);
      Expect(Token::RPAREN);
      return expr;
    }

    case Token::CLASS: {
      BindingPatternUnexpectedToken();
      Consume(Token::CLASS);
      int class_token_pos = position();
      IdentifierT name = impl()->NullIdentifier();
      bool is_strict_reserved_name = false;
      Scanner::Location class_name_location = Scanner::Location::invalid();
      if (peek_any_identifier()) {
        bool is_await = false;
        name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved_name,
                                                   &is_await);
        class_name_location = scanner()->location();
        if (is_await) {
          classifier()->RecordAsyncArrowFormalParametersError(
              scanner()->location(), MessageTemplate::kAwaitBindingIdentifier);
        }
      }
      return ParseClassLiteral(name, class_name_location,
                               is_strict_reserved_name, class_token_pos);
    }

    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      BindingPatternUnexpectedToken();
      return ParseTemplateLiteral(impl()->NullExpression(), beg_pos, false);

    case Token::MOD:
      if (allow_natives() || extension_ != nullptr) {
        BindingPatternUnexpectedToken();
        return ParseV8Intrinsic();
      }
      break;

    case Token::DO:
      if (allow_harmony_do_expressions()) {
        BindingPatternUnexpectedToken();
        return ParseDoExpression();
      }
      break;

    default:
      break;
  }

  ReportUnexpectedToken(Next());
  return impl()->NullExpression();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseExpression() {
  ExpressionClassifier classifier(this);
  ExpressionT result = ParseExpressionCoverGrammar(true);
  ValidateExpression();
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseExpressionCoverGrammar(bool accept_IN) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  ExpressionListT list(pointer_buffer());
  ExpressionT right;
  while (true) {
    ExpressionClassifier binding_classifier(this);
    if (Check(Token::ELLIPSIS)) {
      // 'x, y, ...z' in CoverParenthesizedExpressionAndArrowParameterList only
      // as the formal parameters of'(x, y, ...z) => foo', and is not itself a
      // valid expression.
      classifier()->RecordExpressionError(scanner()->location(),
                                          MessageTemplate::kUnexpectedToken,
                                          Token::String(Token::ELLIPSIS));
      int ellipsis_pos = position();
      int pattern_pos = peek_position();
      ExpressionT pattern = ParseBindingPattern();
      if (peek() == Token::ASSIGN) {
        ReportMessage(MessageTemplate::kRestDefaultInitializer);
        return impl()->NullExpression();
      }
      right = factory()->NewSpread(pattern, ellipsis_pos, pattern_pos);
    } else {
      right = ParseAssignmentExpression(accept_IN);
    }
    // No need to accumulate binding pattern-related errors, since
    // an Expression can't be a binding pattern anyway.
    AccumulateNonBindingPatternErrors();
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    if (!impl()->IsIdentifier(right)) classifier()->RecordNonSimpleParameter();
    list.Add(right);

    if (!Check(Token::COMMA)) break;

    if (right->IsSpread()) {
      classifier()->RecordArrowFormalParametersError(
          scanner()->location(), MessageTemplate::kParamAfterRest);
    }

    if (peek() == Token::RPAREN && PeekAhead() == Token::ARROW) {
      // a trailing comma is allowed at the end of an arrow parameter list
      break;
    }

    // Pass on the 'set_next_function_is_likely_called' flag if we have
    // several function literals separated by comma.
    if (peek() == Token::FUNCTION &&
        function_state_->previous_function_was_likely_called()) {
      function_state_->set_next_function_is_likely_called();
    }
  }

  // Return the single element if the list is empty. We need to do this because
  // callers of this function care about the type of the result if there was
  // only a single assignment expression. The preparser would lose this
  // information otherwise.
  if (list.length() == 1) return right;
  return impl()->ExpressionListToExpression(list);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseArrayLiteral() {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'

  int pos = peek_position();
  ExpressionListT values(pointer_buffer());
  int first_spread_index = -1;
  Consume(Token::LBRACK);
  while (!Check(Token::RBRACK)) {
    ExpressionT elem;
    if (peek() == Token::COMMA) {
      elem = factory()->NewTheHoleLiteral();
    } else if (Check(Token::ELLIPSIS)) {
      int start_pos = position();
      int expr_pos = peek_position();
      ExpressionT argument = ParseAssignmentExpression(true);
      elem = factory()->NewSpread(argument, start_pos, expr_pos);

      if (first_spread_index < 0) {
        first_spread_index = values.length();
      }

      // TODO(verwaest): Remove once we have FailureExpression.
      RETURN_IF_PARSE_ERROR;
      if (argument->IsAssignment()) {
        classifier()->RecordPatternError(
            Scanner::Location(start_pos, end_position()),
            MessageTemplate::kInvalidDestructuringTarget);
      } else {
        CheckDestructuringElement(argument, start_pos, end_position());
      }

      if (peek() == Token::COMMA) {
        classifier()->RecordPatternError(
            Scanner::Location(start_pos, end_position()),
            MessageTemplate::kElementAfterRest);
      }
    } else {
      int beg_pos = peek_position();
      elem = ParseAssignmentExpression(true);
      // TODO(verwaest): Remove once we have FailureExpression.
      RETURN_IF_PARSE_ERROR;
      CheckDestructuringElement(elem, beg_pos, end_position());
    }
    values.Add(elem);
    if (peek() != Token::RBRACK) {
      Expect(Token::COMMA);
    }
  }

  return factory()->NewArrayLiteral(values, first_spread_index, pos);
}

inline bool ParsePropertyKindFromToken(Token::Value token,
                                       ParsePropertyKind* kind) {
  // This returns true, setting the property kind, iff the given token is one
  // which must occur after a property name, indicating that the previous token
  // was in fact a name and not a modifier (like the "get" in "get x").
  switch (token) {
    case Token::COLON:
      *kind = ParsePropertyKind::kValue;
      return true;
    case Token::COMMA:
    case Token::RBRACE:
    case Token::ASSIGN:
      *kind = ParsePropertyKind::kShorthand;
      return true;
    case Token::LPAREN:
      *kind = ParsePropertyKind::kMethod;
      return true;
    case Token::MUL:
    case Token::SEMICOLON:
      *kind = ParsePropertyKind::kClassField;
      return true;
    case Token::PRIVATE_NAME:
      *kind = ParsePropertyKind::kClassField;
      return true;
    default:
      break;
  }
  return false;
}

inline bool ParseAsAccessor(Token::Value token, Token::Value contextual_token,
                            ParsePropertyKind* kind) {
  if (ParsePropertyKindFromToken(token, kind)) return false;

  if (contextual_token == Token::GET) {
    *kind = ParsePropertyKind::kAccessorGetter;
  } else if (contextual_token == Token::SET) {
    *kind = ParsePropertyKind::kAccessorSetter;
  } else {
    return false;
  }

  return true;
}

template <class Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParsePropertyName(
    IdentifierT* name, ParsePropertyKind* kind, ParseFunctionFlags* flags,
    bool* is_computed_name, bool* is_private) {
  DCHECK_EQ(ParsePropertyKind::kNotSet, *kind);
  DCHECK_EQ(*flags, ParseFunctionFlag::kIsNormal);
  DCHECK(!*is_computed_name);

  if (Check(Token::ASYNC)) {
    Token::Value token = peek();
    if ((token != Token::MUL && ParsePropertyKindFromToken(token, kind)) ||
        scanner()->HasLineTerminatorBeforeNext()) {
      *name = impl()->GetSymbol();
      impl()->PushLiteralName(*name);
      return factory()->NewStringLiteral(*name, position());
    }
    *flags = ParseFunctionFlag::kIsAsync;
    *kind = ParsePropertyKind::kMethod;
  }

  if (Check(Token::MUL)) {
    *flags |= ParseFunctionFlag::kIsGenerator;
    *kind = ParsePropertyKind::kMethod;
  }

  if (*kind == ParsePropertyKind::kNotSet && Check(Token::IDENTIFIER) &&
      !ParseAsAccessor(peek(), scanner()->current_contextual_token(), kind)) {
    *name = impl()->GetSymbol();
    impl()->PushLiteralName(*name);
    return factory()->NewStringLiteral(*name, position());
  }

  int pos = peek_position();

  // For non computed property names we normalize the name a bit:
  //
  //   "12" -> 12
  //   12.3 -> "12.3"
  //   12.30 -> "12.3"
  //   identifier -> "identifier"
  //
  // This is important because we use the property name as a key in a hash
  // table when we compute constant properties.
  bool is_array_index;
  uint32_t index;
  switch (peek()) {
    case Token::STRING:
      Consume(Token::STRING);
      *name = impl()->GetSymbol();
      is_array_index = impl()->IsArrayIndex(*name, &index);
      break;

    case Token::SMI:
      Consume(Token::SMI);
      index = scanner()->smi_value();
      is_array_index = true;
      // Token::SMI were scanned from their canonical representation.
      *name = impl()->GetSymbol();
      break;

    case Token::NUMBER: {
      Consume(Token::NUMBER);
      *name = impl()->GetNumberAsSymbol();
      is_array_index = impl()->IsArrayIndex(*name, &index);
      break;
    }
    case Token::LBRACK: {
      *name = impl()->NullIdentifier();
      *is_computed_name = true;
      Consume(Token::LBRACK);
      ExpressionClassifier computed_name_classifier(this);
      ExpressionT expression = ParseAssignmentExpression(true);
      ValidateExpression();
      AccumulateFormalParameterContainmentErrors();
      Expect(Token::RBRACK);
      if (*kind == ParsePropertyKind::kNotSet) {
        ParsePropertyKindFromToken(peek(), kind);
      }
      return expression;
    }

    case Token::ELLIPSIS:
      if (*kind == ParsePropertyKind::kNotSet) {
        *name = impl()->NullIdentifier();
        Consume(Token::ELLIPSIS);
        ExpressionT expression = ParseAssignmentExpression(true);
        *kind = ParsePropertyKind::kSpread;

        // TODO(verwaest): Remove once we have FailureExpression.
        RETURN_IF_PARSE_ERROR;
        if (!impl()->IsIdentifier(expression)) {
          classifier()->RecordBindingPatternError(
              scanner()->location(),
              MessageTemplate::kInvalidRestBindingPattern);
        }

        if (!expression->IsValidReferenceExpression()) {
          classifier()->RecordAssignmentPatternError(
              scanner()->location(),
              MessageTemplate::kInvalidRestAssignmentPattern);
        }

        if (peek() != Token::RBRACE) {
          classifier()->RecordPatternError(scanner()->location(),
                                           MessageTemplate::kElementAfterRest);
        }
        return expression;
      }
      V8_FALLTHROUGH;

    default:
      *name = ParseIdentifierName();
      is_array_index = false;
      break;
  }

  if (*kind == ParsePropertyKind::kNotSet) {
    ParsePropertyKindFromToken(peek(), kind);
  }
  RETURN_IF_PARSE_ERROR;
  impl()->PushLiteralName(*name);
  return is_array_index ? factory()->NewNumberLiteral(index, pos)
                        : factory()->NewStringLiteral(*name, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ClassLiteralPropertyT
ParserBase<Impl>::ParseClassPropertyDefinition(
    ClassLiteralChecker* checker, ClassInfo* class_info, IdentifierT* name,
    bool has_extends, bool* is_computed_name,
    ClassLiteralProperty::Kind* property_kind, bool* is_static,
    bool* is_private) {
  DCHECK_NOT_NULL(class_info);
  ParseFunctionFlags function_flags = ParseFunctionFlag::kIsNormal;
  *is_static = false;
  *property_kind = ClassLiteralProperty::METHOD;
  ParsePropertyKind kind = ParsePropertyKind::kNotSet;

  Token::Value name_token = peek();
  DCHECK_IMPLIES(name_token == Token::PRIVATE_NAME,
                 allow_harmony_private_fields());

  int property_beg_pos = scanner()->peek_location().beg_pos;
  int name_token_position = property_beg_pos;
  *name = impl()->NullIdentifier();
  ExpressionT name_expression;
  if (name_token == Token::STATIC) {
    Consume(Token::STATIC);
    name_token_position = scanner()->peek_location().beg_pos;
    if (peek() == Token::LPAREN) {
      kind = ParsePropertyKind::kMethod;
      *name = impl()->GetSymbol();  // TODO(bakkot) specialize on 'static'
      name_expression = factory()->NewStringLiteral(*name, position());
    } else if (peek() == Token::ASSIGN || peek() == Token::SEMICOLON ||
               peek() == Token::RBRACE) {
      *name = impl()->GetSymbol();  // TODO(bakkot) specialize on 'static'
      name_expression = factory()->NewStringLiteral(*name, position());
    } else if (peek() == Token::PRIVATE_NAME) {
      DCHECK(allow_harmony_private_fields());
      // TODO(gsathya): Make a better error message for this.
      ReportUnexpectedToken(Next());
      return impl()->NullLiteralProperty();
    } else {
      *is_static = true;
      name_expression = ParsePropertyName(name, &kind, &function_flags,
                                          is_computed_name, is_private);
    }
  } else if (name_token == Token::PRIVATE_NAME) {
    Consume(Token::PRIVATE_NAME);
    *is_private = true;
    *name = impl()->GetSymbol();
    name_expression = factory()->NewStringLiteral(*name, position());
  } else {
    name_expression = ParsePropertyName(name, &kind, &function_flags,
                                        is_computed_name, is_private);
  }

  if (!class_info->has_name_static_property && *is_static &&
      impl()->IsName(*name)) {
    class_info->has_name_static_property = true;
  }

  switch (kind) {
    case ParsePropertyKind::kClassField:
    case ParsePropertyKind::kNotSet:  // This case is a name followed by a name
                                      // or other property. Here we have to
                                      // assume that's an uninitialized field
                                      // followed by a linebreak followed by a
                                      // property, with ASI adding the
                                      // semicolon. If not, there will be a
                                      // syntax error after parsing the first
                                      // name as an uninitialized field.
    case ParsePropertyKind::kShorthand:
    case ParsePropertyKind::kValue:
      if (allow_harmony_public_fields() || allow_harmony_private_fields()) {
        *property_kind = ClassLiteralProperty::FIELD;
        *is_private = name_token == Token::PRIVATE_NAME;
        if (*is_static && !allow_harmony_static_fields()) {
          ReportUnexpectedToken(Next());
          return impl()->NullLiteralProperty();
        }
        if (!*is_computed_name) {
          checker->CheckClassFieldName(*is_static);
        }
        ExpressionT initializer = ParseClassFieldInitializer(
            class_info, property_beg_pos, *is_static);
        ExpectSemicolon();
        ClassLiteralPropertyT result = factory()->NewClassLiteralProperty(
            name_expression, initializer, *property_kind, *is_static,
            *is_computed_name, *is_private);
        RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);
        impl()->SetFunctionNameFromPropertyName(result, *name);
        return result;

      } else {
        ReportUnexpectedToken(Next());
        return impl()->NullLiteralProperty();
      }

    case ParsePropertyKind::kMethod: {
      // MethodDefinition
      //    PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
      //    '*' PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
      //    async PropertyName '(' StrictFormalParameters ')'
      //        '{' FunctionBody '}'
      //    async '*' PropertyName '(' StrictFormalParameters ')'
      //        '{' FunctionBody '}'

      if (!*is_computed_name) {
        checker->CheckClassMethodName(name_token, ParsePropertyKind::kMethod,
                                      function_flags, *is_static);
        RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty)
      }

      FunctionKind kind = MethodKindFor(function_flags);

      if (!*is_static && impl()->IsConstructor(*name)) {
        class_info->has_seen_constructor = true;
        kind = has_extends ? FunctionKind::kDerivedConstructor
                           : FunctionKind::kBaseConstructor;
      }

      ExpressionT value = impl()->ParseFunctionLiteral(
          *name, scanner()->location(), kSkipFunctionNameCheck, kind,
          name_token_position, FunctionLiteral::kAccessorOrMethod,
          language_mode(), nullptr);
      RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);

      *property_kind = ClassLiteralProperty::METHOD;
      ClassLiteralPropertyT result = factory()->NewClassLiteralProperty(
          name_expression, value, *property_kind, *is_static, *is_computed_name,
          *is_private);
      impl()->SetFunctionNameFromPropertyName(result, *name);
      return result;
    }

    case ParsePropertyKind::kAccessorGetter:
    case ParsePropertyKind::kAccessorSetter: {
      DCHECK_EQ(function_flags, ParseFunctionFlag::kIsNormal);
      bool is_get = kind == ParsePropertyKind::kAccessorGetter;

      if (!*is_computed_name) {
        checker->CheckClassMethodName(name_token, kind,
                                      ParseFunctionFlag::kIsNormal, *is_static);
        RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty)
        // Make sure the name expression is a string since we need a Name for
        // Runtime_DefineAccessorPropertyUnchecked and since we can determine
        // this statically we can skip the extra runtime check.
        name_expression =
            factory()->NewStringLiteral(*name, name_expression->position());
      }

      FunctionKind kind = is_get ? FunctionKind::kGetterFunction
                                 : FunctionKind::kSetterFunction;

      FunctionLiteralT value = impl()->ParseFunctionLiteral(
          *name, scanner()->location(), kSkipFunctionNameCheck, kind,
          name_token_position, FunctionLiteral::kAccessorOrMethod,
          language_mode(), nullptr);
      RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);

      *property_kind =
          is_get ? ClassLiteralProperty::GETTER : ClassLiteralProperty::SETTER;
      ClassLiteralPropertyT result = factory()->NewClassLiteralProperty(
          name_expression, value, *property_kind, *is_static, *is_computed_name,
          *is_private);
      const AstRawString* prefix =
          is_get ? ast_value_factory()->get_space_string()
                 : ast_value_factory()->set_space_string();
      impl()->SetFunctionNameFromPropertyName(result, *name, prefix);
      return result;
    }
    case ParsePropertyKind::kSpread:
      ReportUnexpectedTokenAt(
          Scanner::Location(name_token_position, name_expression->position()),
          name_token);
      return impl()->NullLiteralProperty();
  }
  UNREACHABLE();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseClassFieldInitializer(ClassInfo* class_info, int beg_pos,
                                             bool is_static) {
  DeclarationScope* initializer_scope = is_static
                                            ? class_info->static_fields_scope
                                            : class_info->instance_fields_scope;

  if (initializer_scope == nullptr) {
    initializer_scope =
        NewFunctionScope(FunctionKind::kClassFieldsInitializerFunction);
    // TODO(gsathya): Make scopes be non contiguous.
    initializer_scope->set_start_position(beg_pos);
    initializer_scope->SetLanguageMode(LanguageMode::kStrict);
  }

  ExpressionT initializer;
  if (Check(Token::ASSIGN)) {
    FunctionState initializer_state(&function_state_, &scope_,
                                    initializer_scope);
    ExpressionClassifier expression_classifier(this);

    initializer = ParseAssignmentExpression(true);
    RETURN_IF_PARSE_ERROR_CUSTOM(NullExpression);
    ValidateExpression();
    RETURN_IF_PARSE_ERROR_CUSTOM(NullExpression);
  } else {
    initializer = factory()->NewUndefinedLiteral(kNoSourcePosition);
  }

  initializer_scope->set_end_position(end_position());
  if (is_static) {
    class_info->static_fields_scope = initializer_scope;
    class_info->has_static_class_fields = true;
  } else {
    class_info->instance_fields_scope = initializer_scope;
    class_info->has_instance_class_fields = true;
  }

  return initializer;
}

template <typename Impl>
typename ParserBase<Impl>::ObjectLiteralPropertyT
ParserBase<Impl>::ParseObjectPropertyDefinition(ObjectLiteralChecker* checker,
                                                bool* is_computed_name,
                                                bool* is_rest_property) {
  ParseFunctionFlags function_flags = ParseFunctionFlag::kIsNormal;
  ParsePropertyKind kind = ParsePropertyKind::kNotSet;

  IdentifierT name = impl()->NullIdentifier();
  Token::Value name_token = peek();
  int next_beg_pos = peek_position();
  int next_end_pos = peek_end_position();

  bool is_private = false;
  ExpressionT name_expression = ParsePropertyName(
      &name, &kind, &function_flags, is_computed_name, &is_private);
  RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);

  if (is_private) {
    // TODO(joyee): private names in object literals should be Syntax Errors
    // https://tc39.github.io/proposal-private-methods/#prod-PropertyDefinition
  }
  switch (kind) {
    case ParsePropertyKind::kSpread:
      DCHECK_EQ(function_flags, ParseFunctionFlag::kIsNormal);
      DCHECK(!*is_computed_name);
      DCHECK_EQ(Token::ELLIPSIS, name_token);

      *is_computed_name = true;
      *is_rest_property = true;

      return factory()->NewObjectLiteralProperty(
          factory()->NewTheHoleLiteral(), name_expression,
          ObjectLiteralProperty::SPREAD, true);

    case ParsePropertyKind::kValue: {
      DCHECK_EQ(function_flags, ParseFunctionFlag::kIsNormal);

      if (!*is_computed_name) {
        checker->CheckDuplicateProto(name_token);
      }
      Consume(Token::COLON);
      int beg_pos = peek_position();
      ExpressionT value = ParseAssignmentExpression(true);
      RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);
      CheckDestructuringElement(value, beg_pos, end_position());

      ObjectLiteralPropertyT result = factory()->NewObjectLiteralProperty(
          name_expression, value, *is_computed_name);
      impl()->SetFunctionNameFromPropertyName(result, name);
      return result;
    }

    case ParsePropertyKind::kShorthand: {
      // PropertyDefinition
      //    IdentifierReference
      //    CoverInitializedName
      //
      // CoverInitializedName
      //    IdentifierReference Initializer?
      DCHECK_EQ(function_flags, ParseFunctionFlag::kIsNormal);

      if (!Token::IsIdentifier(name_token, language_mode(),
                               this->is_generator(),
                               parsing_module_ || is_async_function())) {
        ReportUnexpectedToken(Next());
        return impl()->NullLiteralProperty();
      }

      DCHECK(!*is_computed_name);

      if (impl()->IsEvalOrArguments(name) && is_strict(language_mode())) {
        classifier()->RecordBindingPatternError(
            scanner()->location(), MessageTemplate::kStrictEvalArguments);
      }

      if (name_token == Token::LET) {
        classifier()->RecordLetPatternError(
            scanner()->location(), MessageTemplate::kLetInLexicalBinding);
      }
      if (name_token == Token::AWAIT) {
        DCHECK(!is_async_function());
        classifier()->RecordAsyncArrowFormalParametersError(
            Scanner::Location(next_beg_pos, next_end_pos),
            MessageTemplate::kAwaitBindingIdentifier);
      }
      ExpressionT lhs = impl()->ExpressionFromIdentifier(name, next_beg_pos);
      CheckDestructuringElement(lhs, next_beg_pos, next_end_pos);

      ExpressionT value;
      if (peek() == Token::ASSIGN) {
        Consume(Token::ASSIGN);
        ExpressionClassifier rhs_classifier(this);
        ExpressionT rhs = ParseAssignmentExpression(true);
        RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);
        ValidateExpression();
        RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);
        AccumulateFormalParameterContainmentErrors();
        value = factory()->NewAssignment(Token::ASSIGN, lhs, rhs,
                                         kNoSourcePosition);
        classifier()->RecordExpressionError(
            Scanner::Location(next_beg_pos, end_position()),
            MessageTemplate::kInvalidCoverInitializedName);

        impl()->SetFunctionNameFromIdentifierRef(rhs, lhs);
      } else {
        value = lhs;
      }

      ObjectLiteralPropertyT result = factory()->NewObjectLiteralProperty(
          name_expression, value, ObjectLiteralProperty::COMPUTED, false);
      impl()->SetFunctionNameFromPropertyName(result, name);
      return result;
    }

    case ParsePropertyKind::kMethod: {
      // MethodDefinition
      //    PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
      //    '*' PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'

      classifier()->RecordPatternError(
          Scanner::Location(next_beg_pos, end_position()),
          MessageTemplate::kInvalidDestructuringTarget);

      FunctionKind kind = MethodKindFor(function_flags);

      ExpressionT value = impl()->ParseFunctionLiteral(
          name, scanner()->location(), kSkipFunctionNameCheck, kind,
          next_beg_pos, FunctionLiteral::kAccessorOrMethod, language_mode(),
          nullptr);
      RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);

      ObjectLiteralPropertyT result = factory()->NewObjectLiteralProperty(
          name_expression, value, ObjectLiteralProperty::COMPUTED,
          *is_computed_name);
      impl()->SetFunctionNameFromPropertyName(result, name);
      return result;
    }

    case ParsePropertyKind::kAccessorGetter:
    case ParsePropertyKind::kAccessorSetter: {
      DCHECK_EQ(function_flags, ParseFunctionFlag::kIsNormal);
      bool is_get = kind == ParsePropertyKind::kAccessorGetter;

      classifier()->RecordPatternError(
          Scanner::Location(next_beg_pos, end_position()),
          MessageTemplate::kInvalidDestructuringTarget);

      if (!*is_computed_name) {
        // Make sure the name expression is a string since we need a Name for
        // Runtime_DefineAccessorPropertyUnchecked and since we can determine
        // this statically we can skip the extra runtime check.
        name_expression =
            factory()->NewStringLiteral(name, name_expression->position());
      }

      FunctionKind kind = is_get ? FunctionKind::kGetterFunction
                                 : FunctionKind::kSetterFunction;

      FunctionLiteralT value = impl()->ParseFunctionLiteral(
          name, scanner()->location(), kSkipFunctionNameCheck, kind,
          next_beg_pos, FunctionLiteral::kAccessorOrMethod, language_mode(),
          nullptr);
      RETURN_IF_PARSE_ERROR_CUSTOM(NullLiteralProperty);

      ObjectLiteralPropertyT result = factory()->NewObjectLiteralProperty(
          name_expression, value,
          is_get ? ObjectLiteralProperty::GETTER
                 : ObjectLiteralProperty::SETTER,
          *is_computed_name);
      const AstRawString* prefix =
          is_get ? ast_value_factory()->get_space_string()
                 : ast_value_factory()->set_space_string();
      impl()->SetFunctionNameFromPropertyName(result, name, prefix);
      return result;
    }

    case ParsePropertyKind::kClassField:
    case ParsePropertyKind::kNotSet:
      ReportUnexpectedToken(Next());
      return impl()->NullLiteralProperty();
  }
  UNREACHABLE();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseObjectLiteral() {
  // ObjectLiteral ::
  // '{' (PropertyDefinition (',' PropertyDefinition)* ','? )? '}'

  int pos = peek_position();
  ObjectPropertyListT properties(pointer_buffer());
  int number_of_boilerplate_properties = 0;

  bool has_computed_names = false;
  bool has_rest_property = false;
  ObjectLiteralChecker checker(this);

  Consume(Token::LBRACE);

  while (!Check(Token::RBRACE)) {
    FuncNameInferrerState fni_state(&fni_);

    bool is_computed_name = false;
    bool is_rest_property = false;
    ObjectLiteralPropertyT property = ParseObjectPropertyDefinition(
        &checker, &is_computed_name, &is_rest_property);
    RETURN_IF_PARSE_ERROR;

    if (is_computed_name) {
      has_computed_names = true;
    }

    if (is_rest_property) {
      has_rest_property = true;
    }

    if (impl()->IsBoilerplateProperty(property) && !has_computed_names) {
      // Count CONSTANT or COMPUTED properties to maintain the enumeration
      // order.
      number_of_boilerplate_properties++;
    }

    properties.Add(property);

    if (peek() != Token::RBRACE) {
      Expect(Token::COMMA);
      RETURN_IF_PARSE_ERROR;
    }

    fni_.Infer();
  }

  // In pattern rewriter, we rewrite rest property to call out to a
  // runtime function passing all the other properties as arguments to
  // this runtime function. Here, we make sure that the number of
  // properties is less than number of arguments allowed for a runtime
  // call.
  if (has_rest_property && properties.length() > Code::kMaxArguments) {
    this->classifier()->RecordPatternError(Scanner::Location(pos, position()),
                                           MessageTemplate::kTooManyArguments);
  }

  return impl()->InitializeObjectLiteral(factory()->NewObjectLiteral(
      properties, number_of_boilerplate_properties, pos, has_rest_property));
}

template <typename Impl>
void ParserBase<Impl>::ParseArguments(
    typename ParserBase<Impl>::ExpressionListT* args, bool* has_spread,
    bool maybe_arrow) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  *has_spread = false;
  Consume(Token::LPAREN);

  while (peek() != Token::RPAREN) {
    int start_pos = peek_position();
    bool is_spread = Check(Token::ELLIPSIS);
    int expr_pos = peek_position();

    ExpressionT argument = ParseAssignmentExpression(true);
    RETURN_IF_PARSE_ERROR_VOID;
    if (maybe_arrow) {
      if (!impl()->IsIdentifier(argument)) {
        classifier()->previous()->RecordNonSimpleParameter();
      }
      if (is_spread) {
        classifier()->previous()->RecordNonSimpleParameter();
        if (argument->IsAssignment()) {
          classifier()->RecordAsyncArrowFormalParametersError(
              scanner()->location(), MessageTemplate::kRestDefaultInitializer);
        }
        if (peek() == Token::COMMA) {
          classifier()->RecordAsyncArrowFormalParametersError(
              scanner()->peek_location(), MessageTemplate::kParamAfterRest);
        }
      }
    }
    if (is_spread) {
      *has_spread = true;
      argument = factory()->NewSpread(argument, start_pos, expr_pos);
    }
    args->Add(argument);
    if (!Check(Token::COMMA)) break;
  }

  if (args->length() > Code::kMaxArguments) {
    ReportMessage(MessageTemplate::kTooManyArguments);
    return;
  }

  Scanner::Location location = scanner_->location();
  if (!Check(Token::RPAREN)) {
    impl()->ReportMessageAt(location, MessageTemplate::kUnterminatedArgList);
  }
}

// Precedence = 2
template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseAssignmentExpression(bool accept_IN) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   ArrowFunction
  //   YieldExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression
  int lhs_beg_pos = peek_position();

  if (peek() == Token::YIELD && is_generator()) {
    return ParseYieldExpression(accept_IN);
  }

  FuncNameInferrerState fni_state(&fni_);
  ExpressionClassifier arrow_formals_classifier(this);

  Scope::Snapshot scope_snapshot(scope());
  int rewritable_length = static_cast<int>(
      function_state_->destructuring_assignments_to_rewrite().size());
  bool is_async = peek() == Token::ASYNC && PeekAhead() != Token::ARROW;
  bool parenthesized_formals =
      (is_async ? PeekAhead() : peek()) == Token::LPAREN;

  ExpressionT expression = ParseConditionalExpression(accept_IN);

  if (peek() == Token::ARROW) {
    Scanner::Location arrow_loc = scanner()->peek_location();
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    ValidateArrowFormalParameters(expression, parenthesized_formals, is_async);
    // This reads strangely, but is correct: it checks whether any
    // sub-expression of the parameter list failed to be a valid formal
    // parameter initializer. Since YieldExpressions are banned anywhere
    // in an arrow parameter list, this is correct.
    // TODO(adamk): Rename "FormalParameterInitializerError" to refer to
    // "YieldExpression", which is its only use.
    ValidateFormalParameterInitializer();

    Scanner::Location loc(lhs_beg_pos, end_position());
    DeclarationScope* scope =
        NewFunctionScope(is_async ? FunctionKind::kAsyncArrowFunction
                                  : FunctionKind::kArrowFunction);

    // Because the arrow's parameters were parsed in the outer scope,
    // we need to fix up the scope chain appropriately.
    scope_snapshot.Reparent(scope);

    FormalParametersT parameters(scope);
    if (!classifier()->is_simple_parameter_list()) {
      scope->SetHasNonSimpleParameters();
      parameters.is_simple = false;
    }

    scope->set_start_position(lhs_beg_pos);
    // TODO(verwaest): Disable DCHECKs in failure mode?
    RETURN_IF_PARSE_ERROR;
    impl()->DeclareArrowFunctionFormalParameters(&parameters, expression, loc);

    expression =
        ParseArrowFunctionLiteral(accept_IN, parameters, rewritable_length);
    Accumulate(ExpressionClassifier::AsyncArrowFormalParametersProduction);
    classifier()->RecordPatternError(arrow_loc,
                                     MessageTemplate::kUnexpectedToken,
                                     Token::String(Token::ARROW));

    fni_.Infer();

    return expression;
  }

  // "expression" was not itself an arrow function parameter list, but it might
  // form part of one.  Propagate speculative formal parameter error locations
  // (including those for binding patterns, since formal parameters can
  // themselves contain binding patterns).
  unsigned productions = ExpressionClassifier::AllProductions &
                         ~ExpressionClassifier::ArrowFormalParametersProduction;

  // Parenthesized identifiers and property references are allowed as part
  // of a larger assignment pattern, even though parenthesized patterns
  // themselves are not allowed, e.g., "[(x)] = []". Only accumulate
  // assignment pattern errors if the parsed expression is more complex.
  // TODO(verwaest): Remove once we have FailureExpression.
  RETURN_IF_PARSE_ERROR;
  if (IsValidReferenceExpression(expression)) {
    productions &= ~ExpressionClassifier::AssignmentPatternProduction;
  }

  const bool is_destructuring_assignment =
      IsValidPattern(expression) && peek() == Token::ASSIGN;
  if (is_destructuring_assignment) {
    // This is definitely not an expression so don't accumulate
    // expression-related errors.
    productions &= ~ExpressionClassifier::ExpressionProduction;
    ValidateAssignmentPattern();
  }

  Accumulate(productions);
  if (!Token::IsAssignmentOp(peek())) return expression;

  if (is_destructuring_assignment) {
    impl()->MarkPatternAsAssigned(expression);
  } else {
    expression = CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, end_position(),
        MessageTemplate::kInvalidLhsInAssignment);
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    impl()->MarkExpressionAsAssigned(expression);
  }

  Token::Value op = Next();  // Get assignment operator.
  if (op != Token::ASSIGN) {
    classifier()->RecordPatternError(scanner()->location(),
                                     MessageTemplate::kUnexpectedToken,
                                     Token::String(op));
  }
  int pos = position();

  ExpressionClassifier rhs_classifier(this);

  ExpressionT right = ParseAssignmentExpression(accept_IN);
  ValidateExpression();
  AccumulateFormalParameterContainmentErrors();

  // We try to estimate the set of properties set by constructors. We define a
  // new property whenever there is an assignment to a property of 'this'. We
  // should probably only add properties if we haven't seen them
  // before. Otherwise we'll probably overestimate the number of properties.
  if (op == Token::ASSIGN && impl()->IsThisProperty(expression)) {
    function_state_->AddProperty();
  }

  // TODO(verwaest): Remove once we have FailureExpression.
  RETURN_IF_PARSE_ERROR;
  impl()->CheckAssigningFunctionLiteralToProperty(expression, right);

  // Check if the right hand side is a call to avoid inferring a
  // name if we're dealing with "a = function(){...}();"-like
  // expression.
  if (op == Token::ASSIGN && !right->IsCall() && !right->IsCallNew()) {
    fni_.Infer();
  } else {
    fni_.RemoveLastFunction();
  }

  if (op == Token::ASSIGN) {
    impl()->SetFunctionNameFromIdentifierRef(right, expression);
  }

  DCHECK_NE(op, Token::INIT);
  ExpressionT result = factory()->NewAssignment(op, expression, right, pos);

  if (is_destructuring_assignment) {
    DCHECK_NE(op, Token::ASSIGN_EXP);
    auto rewritable = factory()->NewRewritableExpression(result, scope());
    impl()->QueueDestructuringAssignmentForRewriting(rewritable);
    result = rewritable;
  }

  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseYieldExpression(
    bool accept_IN) {
  // YieldExpression ::
  //   'yield' ([no line terminator] '*'? AssignmentExpression)?
  int pos = peek_position();
  classifier()->RecordPatternError(
      scanner()->peek_location(), MessageTemplate::kInvalidDestructuringTarget);
  classifier()->RecordFormalParameterInitializerError(
      scanner()->peek_location(), MessageTemplate::kYieldInParameter);
  Consume(Token::YIELD);

  CheckStackOverflow();

  // The following initialization is necessary.
  ExpressionT expression = impl()->NullExpression();
  bool delegating = false;  // yield*
  if (!scanner()->HasLineTerminatorBeforeNext()) {
    if (Check(Token::MUL)) delegating = true;
    switch (peek()) {
      case Token::EOS:
      case Token::SEMICOLON:
      case Token::RBRACE:
      case Token::RBRACK:
      case Token::RPAREN:
      case Token::COLON:
      case Token::COMMA:
      case Token::IN:
        // The above set of tokens is the complete set of tokens that can appear
        // after an AssignmentExpression, and none of them can start an
        // AssignmentExpression.  This allows us to avoid looking for an RHS for
        // a regular yield, given only one look-ahead token.
        if (!delegating) break;
        // Delegating yields require an RHS; fall through.
        V8_FALLTHROUGH;
      default:
        expression = ParseAssignmentExpression(accept_IN);
        break;
    }
  }

  if (delegating) {
    ExpressionT yieldstar = factory()->NewYieldStar(expression, pos);
    impl()->RecordSuspendSourceRange(yieldstar, PositionAfterSemicolon());
    function_state_->AddSuspend();
    if (IsAsyncGeneratorFunction(function_state_->kind())) {
      // iterator_close and delegated_iterator_output suspend ids.
      function_state_->AddSuspend();
      function_state_->AddSuspend();
    }
    return yieldstar;
  }

  // Hackily disambiguate o from o.next and o [Symbol.iterator]().
  // TODO(verwaest): Come up with a better solution.
  ExpressionT yield =
      factory()->NewYield(expression, pos, Suspend::kOnExceptionThrow);
  impl()->RecordSuspendSourceRange(yield, PositionAfterSemicolon());
  function_state_->AddSuspend();
  return yield;
}

// Precedence = 3
template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseConditionalExpression(bool accept_IN) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  int pos = peek_position();
  // We start using the binary expression parser for prec >= 4 only!
  ExpressionT expression = ParseBinaryExpression(4, accept_IN);
  return peek() == Token::CONDITIONAL
             ? ParseConditionalContinuation(expression, accept_IN, pos)
             : expression;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseConditionalContinuation(ExpressionT expression,
                                               bool accept_IN, int pos) {
  SourceRange then_range, else_range;
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();

  ExpressionClassifier classifier(this);

  ExpressionT left;
  {
    SourceRangeScope range_scope(scanner(), &then_range);
    Consume(Token::CONDITIONAL);
    // In parsing the first assignment expression in conditional
    // expressions we always accept the 'in' keyword; see ECMA-262,
    // section 11.12, page 58.
    left = ParseAssignmentExpression(true);
  }
  ExpressionT right;
  {
    SourceRangeScope range_scope(scanner(), &else_range);
    Expect(Token::COLON);
    right = ParseAssignmentExpression(accept_IN);
  }
  ExpressionT expr = factory()->NewConditional(expression, left, right, pos);
  impl()->RecordConditionalSourceRange(expr, then_range, else_range);
  AccumulateNonBindingPatternErrors();
  return expr;
}

// Precedence >= 4
template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseBinaryContinuation(ExpressionT x, int prec, int prec1,
                                          bool accept_IN) {
  if (!accept_IN && peek() == Token::IN) return x;
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();
  do {
    // prec1 >= 4
    while (Token::Precedence(peek()) == prec1) {
      SourceRange right_range;
      int pos = peek_position();
      ExpressionT y;
      Token::Value op;
      {
        SourceRangeScope right_range_scope(scanner(), &right_range);
        op = Next();

        const bool is_right_associative = op == Token::EXP;
        const int next_prec = is_right_associative ? prec1 : prec1 + 1;
        y = ParseBinaryExpression(next_prec, accept_IN);
        // TODO(verwaest): Remove once we have FailureExpression.
        RETURN_IF_PARSE_ERROR;
      }

      // For now we distinguish between comparisons and other binary
      // operations.  (We could combine the two and get rid of this
      // code and AST node eventually.)
      if (Token::IsCompareOp(op)) {
        // We have a comparison.
        Token::Value cmp = op;
        switch (op) {
          case Token::NE: cmp = Token::EQ; break;
          case Token::NE_STRICT: cmp = Token::EQ_STRICT; break;
          default: break;
        }
        x = factory()->NewCompareOperation(cmp, x, y, pos);
        if (cmp != op) {
          // The comparison was negated - add a NOT.
          x = factory()->NewUnaryOperation(Token::NOT, x, pos);
        }
      } else if (!impl()->ShortcutNumericLiteralBinaryExpression(&x, y, op,
                                                                 pos) &&
                 !impl()->CollapseNaryExpression(&x, y, op, pos, right_range)) {
        // We have a "normal" binary operation.
        x = factory()->NewBinaryOperation(op, x, y, pos);
        if (op == Token::OR || op == Token::AND) {
          impl()->RecordBinaryOperationSourceRange(x, right_range);
        }
      }
      if (!accept_IN && peek() == Token::IN) return x;
    }
    --prec1;
  } while (prec1 >= prec);

  return x;
}

// Precedence >= 4
template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseBinaryExpression(
    int prec, bool accept_IN) {
  DCHECK_GE(prec, 4);
  ExpressionT x = ParseUnaryExpression();
  int prec1 = Token::Precedence(peek());
  if (prec1 >= prec) {
    return ParseBinaryContinuation(x, prec, prec1, accept_IN);
  }
  return x;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseUnaryOpExpression() {
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();

  Token::Value op = Next();
  int pos = position();

  // Assume "! function ..." indicates the function is likely to be called.
  if (op == Token::NOT && peek() == Token::FUNCTION) {
    function_state_->set_next_function_is_likely_called();
  }

  CheckStackOverflow();

  ExpressionT expression = ParseUnaryExpression();
  RETURN_IF_PARSE_ERROR;

  if (op == Token::DELETE) {
    if (impl()->IsIdentifier(expression) && is_strict(language_mode())) {
      // "delete identifier" is a syntax error in strict mode.
      ReportMessage(MessageTemplate::kStrictDelete);
      return impl()->NullExpression();
    }

    if (impl()->IsPropertyWithPrivateFieldKey(expression)) {
      ReportMessage(MessageTemplate::kDeletePrivateField);
      return impl()->NullExpression();
    }
  }

  if (peek() == Token::EXP) {
    ReportUnexpectedToken(Next());
    return impl()->NullExpression();
  }

  // Allow the parser's implementation to rewrite the expression.
  return impl()->BuildUnaryExpression(expression, op, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParsePrefixExpression() {
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();
  Token::Value op = Next();
  int beg_pos = peek_position();

  CheckStackOverflow();

  ExpressionT expression = ParseUnaryExpression();
  // TODO(verwaest): Remove once we have FailureExpression.
  RETURN_IF_PARSE_ERROR;
  expression = CheckAndRewriteReferenceExpression(
      expression, beg_pos, end_position(),
      MessageTemplate::kInvalidLhsInPrefixOp);
  // TODO(verwaest): Remove once we have FailureExpression.
  RETURN_IF_PARSE_ERROR;
  impl()->MarkExpressionAsAssigned(expression);

  return factory()->NewCountOperation(op, true /* prefix */, expression,
                                      position());
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseAwaitExpression() {
  classifier()->RecordFormalParameterInitializerError(
      scanner()->peek_location(),
      MessageTemplate::kAwaitExpressionFormalParameter);
  int await_pos = peek_position();
  Consume(Token::AWAIT);

  CheckStackOverflow();

  ExpressionT value = ParseUnaryExpression();

  classifier()->RecordBindingPatternError(
      Scanner::Location(await_pos, end_position()),
      MessageTemplate::kInvalidDestructuringTarget);

  ExpressionT expr = factory()->NewAwait(value, await_pos);
  function_state_->AddSuspend();
  impl()->RecordSuspendSourceRange(expr, PositionAfterSemicolon());
  return expr;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseUnaryExpression() {
  // UnaryExpression ::
  //   PostfixExpression
  //   'delete' UnaryExpression
  //   'void' UnaryExpression
  //   'typeof' UnaryExpression
  //   '++' UnaryExpression
  //   '--' UnaryExpression
  //   '+' UnaryExpression
  //   '-' UnaryExpression
  //   '~' UnaryExpression
  //   '!' UnaryExpression
  //   [+Await] AwaitExpression[?Yield]

  Token::Value op = peek();
  if (Token::IsUnaryOp(op)) return ParseUnaryOpExpression();
  if (Token::IsCountOp(op)) return ParsePrefixExpression();
  if (is_async_function() && op == Token::AWAIT) {
    return ParseAwaitExpression();
  }
  return ParsePostfixExpression();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParsePostfixExpression() {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  int lhs_beg_pos = peek_position();
  ExpressionT expression = ParseLeftHandSideExpression();
  if (!scanner()->HasLineTerminatorBeforeNext() && Token::IsCountOp(peek())) {
    BindingPatternUnexpectedToken();
    ArrowFormalParametersUnexpectedToken();

    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    expression = CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, end_position(),
        MessageTemplate::kInvalidLhsInPostfixOp);
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    impl()->MarkExpressionAsAssigned(expression);

    Token::Value next = Next();
    expression =
        factory()->NewCountOperation(next,
                                     false /* postfix */,
                                     expression,
                                     position());
  }
  return expression;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseLeftHandSideExpression() {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  ExpressionT result = ParseMemberWithNewPrefixesExpression();
  if (!Token::IsPropertyOrCall(peek())) return result;
  return ParseLeftHandSideContinuation(result);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseLeftHandSideContinuation(ExpressionT result) {
  DCHECK(Token::IsPropertyOrCall(peek()));
  BindingPatternUnexpectedToken();

  do {
    switch (peek()) {
      case Token::LBRACK: {
        ArrowFormalParametersUnexpectedToken();
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpressionCoverGrammar(true);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK);
        break;
      }

      case Token::LPAREN: {
        int pos;
        // TODO(verwaest): Remove once we have FailureExpression.
        RETURN_IF_PARSE_ERROR;
        if (Token::IsCallable(scanner()->current_token())) {
          // For call of an identifier we want to report position of
          // the identifier as position of the call in the stack trace.
          pos = position();
        } else {
          // For other kinds of calls we record position of the parenthesis as
          // position of the call. Note that this is extremely important for
          // expressions of the form function(){...}() for which call position
          // should not point to the closing brace otherwise it will intersect
          // with positions recorded for function literal and confuse debugger.
          pos = peek_position();
          // Also the trailing parenthesis are a hint that the function will
          // be called immediately. If we happen to have parsed a preceding
          // function literal eagerly, we can also compile it eagerly.
          if (result->IsFunctionLiteral()) {
            result->AsFunctionLiteral()->SetShouldEagerCompile();
            result->AsFunctionLiteral()->mark_as_iife();
          }
        }
        bool has_spread;
        ExpressionListT args(pointer_buffer());
        if (impl()->IsIdentifier(result) &&
            scanner()->current_token() == Token::ASYNC &&
            !scanner()->HasLineTerminatorBeforeNext()) {
          DCHECK(impl()->IsAsync(impl()->AsIdentifier(result)));
          ExpressionClassifier async_classifier(this);
          ParseArguments(&args, &has_spread, true);
          if (peek() == Token::ARROW) {
            RETURN_IF_PARSE_ERROR;
            fni_.RemoveAsyncKeywordFromEnd();
            ValidateBindingPattern();
            ValidateFormalParameterInitializer();
            if (!classifier()->is_valid_async_arrow_formal_parameters()) {
              ReportClassifierError(
                  classifier()->async_arrow_formal_parameters_error());
              return impl()->NullExpression();
            }
            if (args.length()) {
              // async ( Arguments ) => ...
              return impl()->ExpressionListToExpression(args);
            } else {
              // async () => ...
              return factory()->NewEmptyParentheses(pos);
            }
          } else {
            ValidateExpression();
            AccumulateFormalParameterContainmentErrors();
          }
        } else {
          ParseArguments(&args, &has_spread);
        }

        ArrowFormalParametersUnexpectedToken();

        // Keep track of eval() calls since they disable all local variable
        // optimizations.
        // The calls that need special treatment are the
        // direct eval calls. These calls are all of the form eval(...), with
        // no explicit receiver.
        // These calls are marked as potentially direct eval calls. Whether
        // they are actually direct calls to eval is determined at run time.
        Call::PossiblyEval is_possibly_eval =
            CheckPossibleEvalCall(result, scope());

        if (has_spread) {
          result = impl()->SpreadCall(result, args, pos, is_possibly_eval);
        } else {
          result = factory()->NewCall(result, args, pos, is_possibly_eval);
        }

        fni_.RemoveLastFunction();
        break;
      }

      case Token::PERIOD: {
        ArrowFormalParametersUnexpectedToken();
        Consume(Token::PERIOD);
        int pos = position();
        ExpressionT key = ParseIdentifierNameOrPrivateName();
        result = factory()->NewProperty(result, key, pos);
        break;
      }

      default:
        DCHECK(peek() == Token::TEMPLATE_SPAN ||
               peek() == Token::TEMPLATE_TAIL);
        ArrowFormalParametersUnexpectedToken();
        result = ParseTemplateLiteral(result, position(), true);
        break;
    }
  } while (Token::IsPropertyOrCall(peek()));
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseMemberWithPresentNewPrefixesExpression() {
  // NewExpression ::
  //   ('new')+ MemberExpression
  //
  // NewTarget ::
  //   'new' '.' 'target'

  // The grammar for new expressions is pretty warped. We can have several 'new'
  // keywords following each other, and then a MemberExpression. When we see '('
  // after the MemberExpression, it's associated with the rightmost unassociated
  // 'new' to create a NewExpression with arguments. However, a NewExpression
  // can also occur without arguments.

  // Examples of new expression:
  // new foo.bar().baz means (new (foo.bar)()).baz
  // new foo()() means (new foo())()
  // new new foo()() means (new (new foo())())
  // new new foo means new (new foo)
  // new new foo() means new (new foo())
  // new new foo().bar().baz means (new (new foo()).bar()).baz
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();
  Consume(Token::NEW);
  int new_pos = position();
  ExpressionT result;

  CheckStackOverflow();

  if (peek() == Token::SUPER) {
    const bool is_new = true;
    result = ParseSuperExpression(is_new);
  } else if (allow_harmony_dynamic_import() && peek() == Token::IMPORT &&
             (!allow_harmony_import_meta() || PeekAhead() == Token::LPAREN)) {
    impl()->ReportMessageAt(scanner()->peek_location(),
                            MessageTemplate::kImportCallNotNewExpression);
    return impl()->NullExpression();
  } else if (peek() == Token::PERIOD) {
    result = ParseNewTargetExpression();
    return ParseMemberExpressionContinuation(result);
  } else {
    result = ParseMemberWithNewPrefixesExpression();
  }
  if (peek() == Token::LPAREN) {
    // NewExpression with arguments.
    {
      ExpressionListT args(pointer_buffer());
      bool has_spread;
      ParseArguments(&args, &has_spread);

      if (has_spread) {
        result = impl()->SpreadCallNew(result, args, new_pos);
      } else {
        result = factory()->NewCallNew(result, args, new_pos);
      }
    }
    // The expression can still continue with . or [ after the arguments.
    return ParseMemberExpressionContinuation(result);
  }
  // NewExpression without arguments.
  ExpressionListT args(pointer_buffer());
  return factory()->NewCallNew(result, args, new_pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseMemberWithNewPrefixesExpression() {
  return peek() == Token::NEW ? ParseMemberWithPresentNewPrefixesExpression()
                              : ParseMemberExpression();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseFunctionExpression() {
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();

  Consume(Token::FUNCTION);
  int function_token_position = position();

  FunctionKind function_kind = Check(Token::MUL)
                                   ? FunctionKind::kGeneratorFunction
                                   : FunctionKind::kNormalFunction;
  IdentifierT name = impl()->NullIdentifier();
  bool is_strict_reserved_name = false;
  Scanner::Location function_name_location = Scanner::Location::invalid();
  FunctionLiteral::FunctionType function_type =
      FunctionLiteral::kAnonymousExpression;
  if (impl()->ParsingDynamicFunctionDeclaration()) {
    // We don't want dynamic functions to actually declare their name
    // "anonymous". We just want that name in the toString().
    Consume(Token::IDENTIFIER);
    DCHECK(scanner()->CurrentMatchesContextual(Token::ANONYMOUS));
  } else if (peek_any_identifier()) {
    bool is_await = false;
    name = ParseIdentifierOrStrictReservedWord(
        function_kind, &is_strict_reserved_name, &is_await);
    function_name_location = scanner()->location();
    function_type = FunctionLiteral::kNamedExpression;
  }
  return impl()->ParseFunctionLiteral(name, function_name_location,
                                      is_strict_reserved_name
                                          ? kFunctionNameIsStrictReserved
                                          : kFunctionNameValidityUnknown,
                                      function_kind, function_token_position,
                                      function_type, language_mode(), nullptr);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseMemberExpression() {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral | ClassLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments | TemplateLiteral)*
  //
  // CallExpression ::
  //   (SuperCall | ImportCall)
  //     ('[' Expression ']' | '.' Identifier | Arguments | TemplateLiteral)*
  //
  // The '[' Expression ']' and '.' Identifier parts are parsed by
  // ParseMemberExpressionContinuation, and the Arguments part is parsed by the
  // caller.

  // Parse the initial primary or function expression.
  ExpressionT result;
  if (peek() == Token::FUNCTION) {
    result = ParseFunctionExpression();
  } else if (peek() == Token::SUPER) {
    const bool is_new = false;
    result = ParseSuperExpression(is_new);
  } else if (allow_harmony_dynamic_import() && peek() == Token::IMPORT) {
    result = ParseImportExpressions();
  } else {
    result = ParsePrimaryExpression();
  }

  return ParseMemberExpressionContinuation(result);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseImportExpressions() {
  DCHECK(allow_harmony_dynamic_import());

  classifier()->RecordPatternError(scanner()->peek_location(),
                                   MessageTemplate::kUnexpectedToken,
                                   Token::String(Token::IMPORT));

  Consume(Token::IMPORT);
  int pos = position();
  if (allow_harmony_import_meta() && peek() == Token::PERIOD) {
    ExpectMetaProperty(Token::META, "import.meta", pos);
    RETURN_IF_PARSE_ERROR;
    if (!parsing_module_) {
      impl()->ReportMessageAt(scanner()->location(),
                              MessageTemplate::kImportMetaOutsideModule);
      return impl()->NullExpression();
    }

    return impl()->ImportMetaExpression(pos);
  }
  Expect(Token::LPAREN);
  RETURN_IF_PARSE_ERROR;
  if (peek() == Token::RPAREN) {
    impl()->ReportMessageAt(scanner()->location(),
                            MessageTemplate::kImportMissingSpecifier);
    return impl()->NullExpression();
  }
  ExpressionT arg = ParseAssignmentExpression(true);
  RETURN_IF_PARSE_ERROR;
  Expect(Token::RPAREN);
  RETURN_IF_PARSE_ERROR;
  return factory()->NewImportCallExpression(arg, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseSuperExpression(
    bool is_new) {
  Consume(Token::SUPER);
  int pos = position();

  DeclarationScope* scope = GetReceiverScope();
  FunctionKind kind = scope->function_kind();
  if (IsConciseMethod(kind) || IsAccessorFunction(kind) ||
      IsClassConstructor(kind)) {
    if (peek() == Token::PERIOD || peek() == Token::LBRACK) {
      scope->RecordSuperPropertyUsage();
      return impl()->NewSuperPropertyReference(pos);
    }
    // new super() is never allowed.
    // super() is only allowed in derived constructor
    if (!is_new && peek() == Token::LPAREN && IsDerivedConstructor(kind)) {
      // TODO(rossberg): This might not be the correct FunctionState for the
      // method here.
      return impl()->NewSuperCallReference(pos);
    }
  }

  impl()->ReportMessageAt(scanner()->location(),
                          MessageTemplate::kUnexpectedSuper);
  return impl()->NullExpression();
}

template <typename Impl>
void ParserBase<Impl>::ExpectMetaProperty(Token::Value property_name,
                                          const char* full_name, int pos) {
  Consume(Token::PERIOD);
  ExpectContextualKeyword(property_name);
  if (scanner()->literal_contains_escapes()) {
    impl()->ReportMessageAt(Scanner::Location(pos, end_position()),
                            MessageTemplate::kInvalidEscapedMetaProperty,
                            full_name);
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseNewTargetExpression() {
  int pos = position();
  ExpectMetaProperty(Token::TARGET, "new.target", pos);

  classifier()->RecordAssignmentPatternError(
      Scanner::Location(pos, end_position()),
      MessageTemplate::kInvalidDestructuringTarget);

  if (!GetReceiverScope()->is_function_scope()) {
    impl()->ReportMessageAt(scanner()->location(),
                            MessageTemplate::kUnexpectedNewTarget);
    return impl()->NullExpression();
  }

  return impl()->NewTargetExpression(pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::DoParseMemberExpressionContinuation(ExpressionT expression) {
  DCHECK(Token::IsProperty(peek()));
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();
  // Parses this part of MemberExpression:
  // ('[' Expression ']' | '.' Identifier | TemplateLiteral)*
  do {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpressionCoverGrammar(true);
        // TODO(verwaest): Remove once we have FailureExpression.
        RETURN_IF_PARSE_ERROR;
        expression = factory()->NewProperty(expression, index, pos);
        impl()->PushPropertyName(index);
        Expect(Token::RBRACK);
        break;
      }
      case Token::PERIOD: {
        Consume(Token::PERIOD);
        int pos = peek_position();
        ExpressionT key = ParseIdentifierNameOrPrivateName();
        expression = factory()->NewProperty(expression, key, pos);
        break;
      }
      default: {
        DCHECK(peek() == Token::TEMPLATE_SPAN ||
               peek() == Token::TEMPLATE_TAIL);
        int pos;
        if (scanner()->current_token() == Token::IDENTIFIER) {
          pos = position();
        } else {
          pos = peek_position();
          // TODO(verwaest): Remove once we have FailureExpression.
          RETURN_IF_PARSE_ERROR;
          if (expression->IsFunctionLiteral()) {
            // If the tag function looks like an IIFE, set_parenthesized() to
            // force eager compilation.
            expression->AsFunctionLiteral()->SetShouldEagerCompile();
          }
        }
        expression = ParseTemplateLiteral(expression, pos, true);
        break;
      }
    }
  } while (Token::IsProperty(peek()));
  return expression;
}

template <typename Impl>
void ParserBase<Impl>::ParseFormalParameter(FormalParametersT* parameters) {
  // FormalParameter[Yield,GeneratorParameter] :
  //   BindingElement[?Yield, ?GeneratorParameter]
  bool is_rest = parameters->has_rest;

  FuncNameInferrerState fni_state(&fni_);
  ExpressionT pattern = ParseBindingPattern();
  // TODO(verwaest): Remove once we have FailureExpression.
  RETURN_IF_PARSE_ERROR_CUSTOM(Void);
  if (!impl()->IsIdentifier(pattern)) {
    parameters->is_simple = false;
    ValidateFormalParameterInitializer();
  }

  ExpressionT initializer = impl()->NullExpression();
  if (Check(Token::ASSIGN)) {
    if (is_rest) {
      ReportMessage(MessageTemplate::kRestDefaultInitializer);
      return;
    }
    {
      ExpressionClassifier init_classifier(this);
      initializer = ParseAssignmentExpression(true);
      ValidateExpression();
      ValidateFormalParameterInitializer();
      parameters->is_simple = false;
    }
    classifier()->RecordNonSimpleParameter();
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR_CUSTOM(Void);
    impl()->SetFunctionNameFromIdentifierRef(initializer, pattern);
  }

  impl()->AddFormalParameter(parameters, pattern, initializer, end_position(),
                             is_rest);
}

template <typename Impl>
void ParserBase<Impl>::ParseFormalParameterList(FormalParametersT* parameters) {
  // FormalParameters[Yield] :
  //   [empty]
  //   FunctionRestParameter[?Yield]
  //   FormalParameterList[?Yield]
  //   FormalParameterList[?Yield] ,
  //   FormalParameterList[?Yield] , FunctionRestParameter[?Yield]
  //
  // FormalParameterList[Yield] :
  //   FormalParameter[?Yield]
  //   FormalParameterList[?Yield] , FormalParameter[?Yield]

  DCHECK_EQ(0, parameters->arity);

  if (peek() != Token::RPAREN) {
    while (true) {
      if (parameters->arity > Code::kMaxArguments) {
        ReportMessage(MessageTemplate::kTooManyParameters);
        return;
      }
      parameters->has_rest = Check(Token::ELLIPSIS);
      ParseFormalParameter(parameters);

      if (parameters->has_rest) {
        parameters->is_simple = false;
        classifier()->RecordNonSimpleParameter();
        if (peek() == Token::COMMA) {
          impl()->ReportMessageAt(scanner()->peek_location(),
                                  MessageTemplate::kParamAfterRest);
          return;
        }
        break;
      }
      if (!Check(Token::COMMA)) break;
      if (peek() == Token::RPAREN) {
        // allow the trailing comma
        break;
      }
    }
  }

  RETURN_IF_PARSE_ERROR_CUSTOM(Void);
  impl()->DeclareFormalParameters(parameters);
}

template <typename Impl>
typename ParserBase<Impl>::BlockT ParserBase<Impl>::ParseVariableDeclarations(
    VariableDeclarationContext var_context,
    DeclarationParsingResult* parsing_result,
    ZonePtrList<const AstRawString>* names) {
  // VariableDeclarations ::
  //   ('var' | 'const' | 'let') (Identifier ('=' AssignmentExpression)?)+[',']
  //
  // ES6:
  // FIXME(marja, nikolaos): Add an up-to-date comment about ES6 variable
  // declaration syntax.

  DCHECK_NOT_NULL(parsing_result);
  parsing_result->descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
  parsing_result->descriptor.declaration_pos = peek_position();
  parsing_result->descriptor.initialization_pos = peek_position();

  BlockT init_block = impl()->NullStatement();
  if (var_context != kForStatement) {
    init_block = factory()->NewBlock(1, true);
  }

  switch (peek()) {
    case Token::VAR:
      parsing_result->descriptor.mode = VariableMode::kVar;
      Consume(Token::VAR);
      break;
    case Token::CONST:
      Consume(Token::CONST);
      DCHECK_NE(var_context, kStatement);
      parsing_result->descriptor.mode = VariableMode::kConst;
      break;
    case Token::LET:
      Consume(Token::LET);
      DCHECK_NE(var_context, kStatement);
      parsing_result->descriptor.mode = VariableMode::kLet;
      break;
    default:
      UNREACHABLE();  // by current callers
      break;
  }

  parsing_result->descriptor.scope = scope();

  int bindings_start = peek_position();
  do {
    // Parse binding pattern.
    FuncNameInferrerState fni_state(&fni_);

    ExpressionT pattern = impl()->NullExpression();
    int decl_pos = peek_position();
    {
      ExpressionClassifier pattern_classifier(this);
      pattern = ParseBindingPattern();
      RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);

      if (IsLexicalVariableMode(parsing_result->descriptor.mode)) {
        ValidateLetPattern();
      }
    }
    Scanner::Location variable_loc = scanner()->location();

    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);
    bool single_name = impl()->IsIdentifier(pattern);
    if (single_name) {
      impl()->PushVariableName(impl()->AsIdentifier(pattern));
    }

    ExpressionT value = impl()->NullExpression();
    int initializer_position = kNoSourcePosition;
    int value_beg_position = kNoSourcePosition;
    if (Check(Token::ASSIGN)) {
      value_beg_position = peek_position();

      ExpressionClassifier classifier(this);
      value = ParseAssignmentExpression(var_context != kForStatement);
      ValidateExpression();
      variable_loc.end_pos = end_position();

      if (!parsing_result->first_initializer_loc.IsValid()) {
        parsing_result->first_initializer_loc = variable_loc;
      }

      // TODO(verwaest): Remove once we have FailureExpression.
      RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);

      // Don't infer if it is "a = function(){...}();"-like expression.
      if (single_name) {
        if (!value->IsCall() && !value->IsCallNew()) {
          fni_.Infer();
        } else {
          fni_.RemoveLastFunction();
        }
      }

      impl()->SetFunctionNameFromIdentifierRef(value, pattern);

      // End position of the initializer is after the assignment expression.
      initializer_position = end_position();
    } else {
      if (var_context != kForStatement || !PeekInOrOf()) {
        // ES6 'const' and binding patterns require initializers.
        if (parsing_result->descriptor.mode == VariableMode::kConst ||
            !impl()->IsIdentifier(pattern)) {
          impl()->ReportMessageAt(
              Scanner::Location(decl_pos, end_position()),
              MessageTemplate::kDeclarationMissingInitializer,
              !impl()->IsIdentifier(pattern) ? "destructuring" : "const");
          return impl()->NullStatement();
        }
        // 'let x' initializes 'x' to undefined.
        if (parsing_result->descriptor.mode == VariableMode::kLet) {
          value = factory()->NewUndefinedLiteral(position());
        }
      }

      // End position of the initializer is after the variable.
      initializer_position = position();
    }

    typename DeclarationParsingResult::Declaration decl(
        pattern, initializer_position, value);
    decl.value_beg_position = value_beg_position;
    if (var_context == kForStatement) {
      // Save the declaration for further handling in ParseForStatement.
      parsing_result->declarations.push_back(decl);
    } else {
      // Immediately declare the variable otherwise. This avoids O(N^2)
      // behavior (where N is the number of variables in a single
      // declaration) in the PatternRewriter having to do with removing
      // and adding VariableProxies to the Scope (see bug 4699).
      impl()->DeclareAndInitializeVariables(
          init_block, &parsing_result->descriptor, &decl, names);
    }
  } while (Check(Token::COMMA));

  parsing_result->bindings_loc =
      Scanner::Location(bindings_start, end_position());

  return init_block;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseFunctionDeclaration() {
  Consume(Token::FUNCTION);

  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlag::kIsNormal;
  if (Check(Token::MUL)) {
    impl()->ReportMessageAt(
        scanner()->location(),
        MessageTemplate::kGeneratorInSingleStatementContext);
    return impl()->NullStatement();
  }
  return ParseHoistableDeclaration(pos, flags, nullptr, false);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseHoistableDeclaration(
    ZonePtrList<const AstRawString>* names, bool default_export) {
  Consume(Token::FUNCTION);

  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlag::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlag::kIsGenerator;
  }
  return ParseHoistableDeclaration(pos, flags, names, default_export);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseHoistableDeclaration(
    int pos, ParseFunctionFlags flags, ZonePtrList<const AstRawString>* names,
    bool default_export) {
  CheckStackOverflow();

  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //   'function' '(' FormalParameters ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //   'function' '*' '(' FormalParameters ')' '{' FunctionBody '}'
  //
  // The anonymous forms are allowed iff [default_export] is true.
  //
  // 'function' and '*' (if present) have been consumed by the caller.

  DCHECK_IMPLIES((flags & ParseFunctionFlag::kIsAsync) != 0,
                 (flags & ParseFunctionFlag::kIsGenerator) == 0);

  if ((flags & ParseFunctionFlag::kIsAsync) != 0 && Check(Token::MUL)) {
    // Async generator
    flags |= ParseFunctionFlag::kIsGenerator;
  }

  IdentifierT name;
  FunctionNameValidity name_validity;
  IdentifierT variable_name;
  if (default_export && peek() == Token::LPAREN) {
    impl()->GetDefaultStrings(&name, &variable_name);
    name_validity = kSkipFunctionNameCheck;
  } else {
    bool is_strict_reserved;
    bool is_await = false;
    name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved, &is_await);
    name_validity = is_strict_reserved ? kFunctionNameIsStrictReserved
                                       : kFunctionNameValidityUnknown;
    variable_name = name;
  }

  FuncNameInferrerState fni_state(&fni_);
  // TODO(verwaest): Remove once we have FailureIdentifier.
  RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);
  impl()->PushEnclosingName(name);

  FunctionKind kind = FunctionKindFor(flags);

  FunctionLiteralT function = impl()->ParseFunctionLiteral(
      name, scanner()->location(), name_validity, kind, pos,
      FunctionLiteral::kDeclaration, language_mode(), nullptr);

  // In ES6, a function behaves as a lexical binding, except in
  // a script scope, or the initial scope of eval or another function.
  VariableMode mode =
      (!scope()->is_declaration_scope() || scope()->is_module_scope())
          ? VariableMode::kLet
          : VariableMode::kVar;
  // Async functions don't undergo sloppy mode block scoped hoisting, and don't
  // allow duplicates in a block. Both are represented by the
  // sloppy_block_function_map. Don't add them to the map for async functions.
  // Generators are also supposed to be prohibited; currently doing this behind
  // a flag and UseCounting violations to assess web compatibility.
  bool is_sloppy_block_function = is_sloppy(language_mode()) &&
                                  !scope()->is_declaration_scope() &&
                                  flags == ParseFunctionFlag::kIsNormal;

  RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);
  return impl()->DeclareFunction(variable_name, function, mode, pos,
                                 is_sloppy_block_function, names);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseClassDeclaration(
    ZonePtrList<const AstRawString>* names, bool default_export) {
  // ClassDeclaration ::
  //   'class' Identifier ('extends' LeftHandExpression)? '{' ClassBody '}'
  //   'class' ('extends' LeftHandExpression)? '{' ClassBody '}'
  //
  // The anonymous form is allowed iff [default_export] is true.
  //
  // 'class' is expected to be consumed by the caller.
  //
  // A ClassDeclaration
  //
  //   class C { ... }
  //
  // has the same semantics as:
  //
  //   let C = class C { ... };
  //
  // so rewrite it as such.

  int class_token_pos = position();
  IdentifierT name = impl()->NullIdentifier();
  bool is_strict_reserved = false;
  IdentifierT variable_name = impl()->NullIdentifier();
  if (default_export && (peek() == Token::EXTENDS || peek() == Token::LBRACE)) {
    impl()->GetDefaultStrings(&name, &variable_name);
  } else {
    bool is_await = false;
    name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved, &is_await);
    variable_name = name;
  }

  ExpressionClassifier no_classifier(this);
  ExpressionT value = ParseClassLiteral(name, scanner()->location(),
                                        is_strict_reserved, class_token_pos);
  int end_pos = position();
  RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);
  return impl()->DeclareClass(variable_name, value, names, class_token_pos,
                              end_pos);
}

// Language extension which is only enabled for source files loaded
// through the API's extension mechanism.  A native function
// declaration is resolved by looking up the function through a
// callback provided by the extension.
template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseNativeDeclaration() {
  function_state_->DisableOptimization(BailoutReason::kNativeFunctionLiteral);

  int pos = peek_position();
  Consume(Token::FUNCTION);
  // Allow "eval" or "arguments" for backward compatibility.
  IdentifierT name = ParseIdentifier(kAllowRestrictedIdentifiers);
  Expect(Token::LPAREN);
  if (peek() != Token::RPAREN) {
    do {
      ParseIdentifier(kAllowRestrictedIdentifiers);
    } while (Check(Token::COMMA));
  }
  Expect(Token::RPAREN);
  Expect(Token::SEMICOLON);
  RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);
  return impl()->DeclareNative(name, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseAsyncFunctionDeclaration(
    ZonePtrList<const AstRawString>* names, bool default_export) {
  // AsyncFunctionDeclaration ::
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  DCHECK(!scanner()->HasLineTerminatorBeforeNext());
  Consume(Token::FUNCTION);
  ParseFunctionFlags flags = ParseFunctionFlag::kIsAsync;
  return ParseHoistableDeclaration(pos, flags, names, default_export);
}

template <typename Impl>
void ParserBase<Impl>::ParseFunctionBody(
    typename ParserBase<Impl>::StatementListT result, IdentifierT function_name,
    int pos, const FormalParametersT& parameters, FunctionKind kind,
    FunctionLiteral::FunctionType function_type, FunctionBodyType body_type,
    bool accept_IN) {
  DeclarationScope* function_scope = scope()->AsDeclarationScope();
  DeclarationScope* inner_scope = function_scope;
  BlockT inner_block = impl()->NullStatement();

  StatementListT body = result;
  if (!parameters.is_simple) {
    inner_scope = NewVarblockScope();
    inner_scope->set_start_position(scanner()->location().beg_pos);
    inner_block = factory()->NewBlock(8, true);
    inner_block->set_scope(inner_scope);
    body = inner_block->statements();
  }

  {
    BlockState block_state(&scope_, inner_scope);

    if (IsResumableFunction(kind)) impl()->PrepareGeneratorVariables();

    if (body_type == FunctionBodyType::kExpression) {
      ExpressionClassifier classifier(this);
      ExpressionT expression = ParseAssignmentExpression(accept_IN);
      ValidateExpression();

      // TODO(verwaest): Remove once we have FailureExpression.
      RETURN_IF_PARSE_ERROR_VOID;
      if (IsAsyncFunction(kind)) {
        BlockT block = factory()->NewBlock(1, true);
        impl()->RewriteAsyncFunctionBody(body, block, expression);
      } else {
        body->Add(BuildReturnStatement(expression, expression->position()),
                  zone());
      }
    } else {
      DCHECK(accept_IN);
      DCHECK_EQ(FunctionBodyType::kBlock, body_type);
      // If we are parsing the source as if it is wrapped in a function, the
      // source ends without a closing brace.
      Token::Value closing_token = function_type == FunctionLiteral::kWrapped
                                       ? Token::EOS
                                       : Token::RBRACE;

      if (IsAsyncGeneratorFunction(kind)) {
        impl()->ParseAndRewriteAsyncGeneratorFunctionBody(pos, kind, body);
      } else if (IsGeneratorFunction(kind)) {
        impl()->ParseAndRewriteGeneratorFunctionBody(pos, kind, body);
      } else if (IsAsyncFunction(kind)) {
        ParseAsyncFunctionBody(inner_scope, body);
      } else {
        ParseStatementList(body, closing_token);
      }

      if (IsDerivedConstructor(kind)) {
        body->Add(factory()->NewReturnStatement(impl()->ThisExpression(),
                                                kNoSourcePosition),
                  zone());
      }
      Expect(closing_token);
    }
  }

  scope()->set_end_position(end_position());

  bool allow_duplicate_parameters = false;

  if (parameters.is_simple) {
    DCHECK_EQ(inner_scope, function_scope);
    if (is_sloppy(function_scope->language_mode())) {
      impl()->InsertSloppyBlockFunctionVarBindings(function_scope);
    }
    allow_duplicate_parameters = is_sloppy(function_scope->language_mode()) &&
                                 !IsConciseMethod(kind) &&
                                 !IsArrowFunction(kind);
  } else {
    DCHECK_NOT_NULL(inner_scope);
    DCHECK_EQ(function_scope, scope());
    DCHECK_EQ(function_scope, inner_scope->outer_scope());
    impl()->SetLanguageMode(function_scope, inner_scope->language_mode());
    // TODO(verwaest): Disable DCHECKs in failure mode?
    RETURN_IF_PARSE_ERROR_VOID;
    BlockT init_block = impl()->BuildParameterInitializationBlock(parameters);

    if (is_sloppy(inner_scope->language_mode())) {
      impl()->InsertSloppyBlockFunctionVarBindings(inner_scope);
    }

    // TODO(littledan): Merge the two rejection blocks into one
    if (IsAsyncFunction(kind) && !IsAsyncGeneratorFunction(kind)) {
      init_block = impl()->BuildRejectPromiseOnException(init_block);
    }

    inner_scope->set_end_position(end_position());
    if (inner_scope->FinalizeBlockScope() != nullptr) {
      impl()->CheckConflictingVarDeclarations(inner_scope);
      impl()->InsertShadowingVarBindingInitializers(inner_block);
    } else {
      inner_block->set_scope(nullptr);
    }
    inner_scope = nullptr;

    result->Add(init_block, zone());
    result->Add(inner_block, zone());
  }

  ValidateFormalParameters(language_mode(), allow_duplicate_parameters);

  if (!IsArrowFunction(kind)) {
    // Declare arguments after parsing the function since lexical 'arguments'
    // masks the arguments object. Declare arguments before declaring the
    // function var since the arguments object masks 'function arguments'.
    function_scope->DeclareArguments(ast_value_factory());
  }

  RETURN_IF_PARSE_ERROR_VOID;
  impl()->DeclareFunctionNameVar(function_name, function_type, function_scope);
}

template <typename Impl>
void ParserBase<Impl>::CheckArityRestrictions(int param_count,
                                              FunctionKind function_kind,
                                              bool has_rest,
                                              int formals_start_pos,
                                              int formals_end_pos) {
  if (IsGetterFunction(function_kind)) {
    if (param_count != 0) {
      impl()->ReportMessageAt(
          Scanner::Location(formals_start_pos, formals_end_pos),
          MessageTemplate::kBadGetterArity);
    }
  } else if (IsSetterFunction(function_kind)) {
    if (param_count != 1) {
      impl()->ReportMessageAt(
          Scanner::Location(formals_start_pos, formals_end_pos),
          MessageTemplate::kBadSetterArity);
    }
    if (has_rest) {
      impl()->ReportMessageAt(
          Scanner::Location(formals_start_pos, formals_end_pos),
          MessageTemplate::kBadSetterRestParameter);
    }
  }
}

template <typename Impl>
bool ParserBase<Impl>::IsNextLetKeyword() {
  DCHECK_EQ(Token::LET, peek());
  Token::Value next_next = PeekAhead();
  switch (next_next) {
    case Token::LBRACE:
    case Token::LBRACK:
    case Token::IDENTIFIER:
    case Token::STATIC:
    case Token::LET:  // `let let;` is disallowed by static semantics, but the
                      // token must be first interpreted as a keyword in order
                      // for those semantics to apply. This ensures that ASI is
                      // not honored when a LineTerminator separates the
                      // tokens.
    case Token::YIELD:
    case Token::AWAIT:
    case Token::ASYNC:
      return true;
    case Token::FUTURE_STRICT_RESERVED_WORD:
      return is_sloppy(language_mode());
    default:
      return false;
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseArrowFunctionLiteral(
    bool accept_IN, const FormalParametersT& formal_parameters,
    int rewritable_length) {
  const RuntimeCallCounterId counters[2][2] = {
      {RuntimeCallCounterId::kParseBackgroundArrowFunctionLiteral,
       RuntimeCallCounterId::kParseArrowFunctionLiteral},
      {RuntimeCallCounterId::kPreParseBackgroundArrowFunctionLiteral,
       RuntimeCallCounterId::kPreParseArrowFunctionLiteral}};
  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_,
      counters[Impl::IsPreParser()][parsing_on_main_thread_]);
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  DCHECK_EQ(Token::ARROW, peek());
  if (scanner_->HasLineTerminatorBeforeNext()) {
    // ASI inserts `;` after arrow parameters if a line terminator is found.
    // `=> ...` is never a valid expression, so report as syntax error.
    // If next token is not `=>`, it's a syntax error anyways.
    ReportUnexpectedTokenAt(scanner_->peek_location(), Token::ARROW);
    return impl()->NullExpression();
  }

  StatementListT body = impl()->NullStatementList();
  int expected_property_count = -1;
  int suspend_count = 0;
  int function_literal_id = GetNextFunctionLiteralId();

  FunctionKind kind = formal_parameters.scope->function_kind();
  FunctionLiteral::EagerCompileHint eager_compile_hint =
      default_eager_compile_hint_;
  bool can_preparse = impl()->parse_lazily() &&
                      eager_compile_hint == FunctionLiteral::kShouldLazyCompile;
  // TODO(marja): consider lazy-parsing inner arrow functions too. is_this
  // handling in Scope::ResolveVariable needs to change.
  bool is_lazy_top_level_function =
      can_preparse && impl()->AllowsLazyParsingWithoutUnresolvedVariables();
  bool has_braces = true;
  ProducedPreParsedScopeData* produced_preparsed_scope_data = nullptr;
  {
    FunctionState function_state(&function_state_, &scope_,
                                 formal_parameters.scope);

    // Move any queued destructuring assignments which appeared
    // in this function's parameter list into its own function_state.
    function_state.AdoptDestructuringAssignmentsFromParentState(
        rewritable_length);

    Consume(Token::ARROW);

    if (peek() == Token::LBRACE) {
      // Multiple statement body
      DCHECK_EQ(scope(), formal_parameters.scope);
      if (is_lazy_top_level_function) {
        // FIXME(marja): Arrow function parameters will be parsed even if the
        // body is preparsed; move relevant parts of parameter handling to
        // simulate consistent parameter handling.

        // For arrow functions, we don't need to retrieve data about function
        // parameters.
        int dummy_num_parameters = -1;
        DCHECK_NE(kind & FunctionKind::kArrowFunction, 0);
        FunctionLiteral::EagerCompileHint hint;
        bool did_preparse_successfully = impl()->SkipFunction(
            nullptr, kind, FunctionLiteral::kAnonymousExpression,
            formal_parameters.scope, &dummy_num_parameters,
            &produced_preparsed_scope_data, false, &hint);

        // Validate parameter names. We can do this only after preparsing the
        // function, since the function can declare itself strict.
        ValidateFormalParameters(language_mode(), false);
        RETURN_IF_PARSE_ERROR;

        DCHECK_NULL(produced_preparsed_scope_data);

        if (did_preparse_successfully) {
          // Discard any queued destructuring assignments which appeared
          // in this function's parameter list, and which were adopted
          // into this function state, above.
          function_state.RewindDestructuringAssignments(0);
        } else {
          // In case we did not sucessfully preparse the function because of an
          // unidentified error we do a full reparse to return the error.
          Consume(Token::LBRACE);
          body = impl()->NewStatementList(8);
          ParseFunctionBody(body, impl()->NullIdentifier(), kNoSourcePosition,
                            formal_parameters, kind,
                            FunctionLiteral::kAnonymousExpression,
                            FunctionBodyType::kBlock, true);
          CHECK(has_error());
          return impl()->NullExpression();
        }
      } else {
        Consume(Token::LBRACE);
        body = impl()->NewStatementList(8);
        ParseFunctionBody(body, impl()->NullIdentifier(), kNoSourcePosition,
                          formal_parameters, kind,
                          FunctionLiteral::kAnonymousExpression,
                          FunctionBodyType::kBlock, true);
        expected_property_count = function_state.expected_property_count();
      }
    } else {
      // Single-expression body
      has_braces = false;
      body = impl()->NewStatementList(1);
      ParseFunctionBody(body, impl()->NullIdentifier(), kNoSourcePosition,
                        formal_parameters, kind,
                        FunctionLiteral::kAnonymousExpression,
                        FunctionBodyType::kExpression, accept_IN);
      expected_property_count = function_state.expected_property_count();
    }

    formal_parameters.scope->set_end_position(end_position());

    // Validate strict mode.
    if (is_strict(language_mode())) {
      CheckStrictOctalLiteral(formal_parameters.scope->start_position(),
                              end_position());
    }
    impl()->CheckConflictingVarDeclarations(formal_parameters.scope);

    impl()->RewriteDestructuringAssignments();
    suspend_count = function_state.suspend_count();
  }

  FunctionLiteralT function_literal = factory()->NewFunctionLiteral(
      impl()->EmptyIdentifierString(), formal_parameters.scope, body,
      expected_property_count, formal_parameters.num_parameters(),
      formal_parameters.function_length,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression, eager_compile_hint,
      formal_parameters.scope->start_position(), has_braces,
      function_literal_id, produced_preparsed_scope_data);

  function_literal->set_suspend_count(suspend_count);
  function_literal->set_function_token_position(
      formal_parameters.scope->start_position());

  impl()->AddFunctionForNameInference(function_literal);

  if (V8_UNLIKELY((FLAG_log_function_events))) {
    Scope* scope = formal_parameters.scope;
    double ms = timer.Elapsed().InMillisecondsF();
    const char* event_name =
        is_lazy_top_level_function ? "preparse-no-resolution" : "parse";
    const char* name = "arrow function";
    logger_->FunctionEvent(event_name, script_id(), ms, scope->start_position(),
                           scope->end_position(), name, strlen(name));
  }

  return function_literal;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseClassLiteral(
    IdentifierT name, Scanner::Location class_name_location,
    bool name_is_strict_reserved, int class_token_pos) {
  bool is_anonymous = impl()->IsNull(name);

  // All parts of a ClassDeclaration and ClassExpression are strict code.
  if (!is_anonymous) {
    if (name_is_strict_reserved) {
      impl()->ReportMessageAt(class_name_location,
                              MessageTemplate::kUnexpectedStrictReserved);
      return impl()->NullExpression();
    }
    if (impl()->IsEvalOrArguments(name)) {
      impl()->ReportMessageAt(class_name_location,
                              MessageTemplate::kStrictEvalArguments);
      return impl()->NullExpression();
    }
  }

  Scope* block_scope = NewScope(BLOCK_SCOPE);
  BlockState block_state(&scope_, block_scope);
  RaiseLanguageMode(LanguageMode::kStrict);

  ClassInfo class_info(this);
  class_info.is_anonymous = is_anonymous;
  impl()->DeclareClassVariable(name, &class_info, class_token_pos);

  scope()->set_start_position(end_position());
  if (Check(Token::EXTENDS)) {
    FuncNameInferrerState fni_state(&fni_);
    ExpressionClassifier extends_classifier(this);
    class_info.extends = ParseLeftHandSideExpression();
    ValidateExpression();
    AccumulateFormalParameterContainmentErrors();
  }

  ClassLiteralChecker checker(this);

  Expect(Token::LBRACE);

  const bool has_extends = !impl()->IsNull(class_info.extends);
  while (peek() != Token::RBRACE) {
    if (Check(Token::SEMICOLON)) continue;
    FuncNameInferrerState fni_state(&fni_);
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    bool is_static;
    bool is_private = false;
    ClassLiteralProperty::Kind property_kind;
    ExpressionClassifier property_classifier(this);
    IdentifierT property_name;
    // If we haven't seen the constructor yet, it potentially is the next
    // property.
    bool is_constructor = !class_info.has_seen_constructor;
    ClassLiteralPropertyT property = ParseClassPropertyDefinition(
        &checker, &class_info, &property_name, has_extends, &is_computed_name,
        &property_kind, &is_static, &is_private);
    RETURN_IF_PARSE_ERROR;
    if (!class_info.has_static_computed_names && is_static &&
        is_computed_name) {
      class_info.has_static_computed_names = true;
    }
    if (is_computed_name && !is_private &&
        property_kind == ClassLiteralProperty::FIELD) {
      class_info.computed_field_count++;
    }
    is_constructor &= class_info.has_seen_constructor;
    ValidateExpression();
    AccumulateFormalParameterContainmentErrors();

    RETURN_IF_PARSE_ERROR;
    impl()->DeclareClassProperty(name, property, property_name, property_kind,
                                 is_static, is_constructor, is_computed_name,
                                 is_private, &class_info);
    impl()->InferFunctionName();
  }

  Expect(Token::RBRACE);
  int end_pos = end_position();
  block_scope->set_end_position(end_pos);
  return impl()->RewriteClassLiteral(block_scope, name, &class_info,
                                     class_token_pos, end_pos);
}

template <typename Impl>
void ParserBase<Impl>::ParseAsyncFunctionBody(Scope* scope,
                                              StatementListT body) {
  BlockT block = factory()->NewBlock(8, true);

  ParseStatementList(block->statements(), Token::RBRACE);
  impl()->RewriteAsyncFunctionBody(
      body, block, factory()->NewUndefinedLiteral(kNoSourcePosition));
  scope->set_end_position(end_position());
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseAsyncFunctionLiteral() {
  // AsyncFunctionLiteral ::
  //   async [no LineTerminator here] function ( FormalParameters[Await] )
  //       { AsyncFunctionBody }
  //
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  Consume(Token::FUNCTION);
  bool is_strict_reserved = false;
  IdentifierT name = impl()->NullIdentifier();
  FunctionLiteral::FunctionType type = FunctionLiteral::kAnonymousExpression;

  ParseFunctionFlags flags = ParseFunctionFlag::kIsAsync;
  if (Check(Token::MUL)) flags |= ParseFunctionFlag::kIsGenerator;
  const FunctionKind kind = FunctionKindFor(flags);

  if (impl()->ParsingDynamicFunctionDeclaration()) {
    // We don't want dynamic functions to actually declare their name
    // "anonymous". We just want that name in the toString().

    // Consuming token we did not peek yet, which could lead to a ILLEGAL token
    // in the case of a stackoverflow.
    Expect(Token::IDENTIFIER);
    RETURN_IF_PARSE_ERROR;
    DCHECK(scanner()->CurrentMatchesContextual(Token::ANONYMOUS));
  } else if (peek_any_identifier()) {
    type = FunctionLiteral::kNamedExpression;
    bool is_await = false;
    name = ParseIdentifierOrStrictReservedWord(kind, &is_strict_reserved,
                                               &is_await);
    // If the function name is "await", ParseIdentifierOrStrictReservedWord
    // recognized the error.
    DCHECK(!is_await);
  }
  return impl()->ParseFunctionLiteral(
      name, scanner()->location(),
      is_strict_reserved ? kFunctionNameIsStrictReserved
                         : kFunctionNameValidityUnknown,
      kind, pos, type, language_mode(), nullptr);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseTemplateLiteral(
    ExpressionT tag, int start, bool tagged) {
  // A TemplateLiteral is made up of 0 or more TEMPLATE_SPAN tokens (literal
  // text followed by a substitution expression), finalized by a single
  // TEMPLATE_TAIL.
  //
  // In terms of draft language, TEMPLATE_SPAN may be either the TemplateHead or
  // TemplateMiddle productions, while TEMPLATE_TAIL is either TemplateTail, or
  // NoSubstitutionTemplate.
  //
  // When parsing a TemplateLiteral, we must have scanned either an initial
  // TEMPLATE_SPAN, or a TEMPLATE_TAIL.
  DCHECK(peek() == Token::TEMPLATE_SPAN || peek() == Token::TEMPLATE_TAIL);

  if (tagged) {
    // TaggedTemplate expressions prevent the eval compilation cache from being
    // used. This flag is only used if an eval is being parsed.
    set_allow_eval_cache(false);
  }

  bool forbid_illegal_escapes = !tagged;

  // If we reach a TEMPLATE_TAIL first, we are parsing a NoSubstitutionTemplate.
  // In this case we may simply consume the token and build a template with a
  // single TEMPLATE_SPAN and no expressions.
  if (peek() == Token::TEMPLATE_TAIL) {
    Consume(Token::TEMPLATE_TAIL);
    int pos = position();
    typename Impl::TemplateLiteralState ts = impl()->OpenTemplateLiteral(pos);
    bool is_valid = CheckTemplateEscapes(forbid_illegal_escapes);
    impl()->AddTemplateSpan(&ts, is_valid, true);
    return impl()->CloseTemplateLiteral(&ts, start, tag);
  }

  Consume(Token::TEMPLATE_SPAN);
  int pos = position();
  typename Impl::TemplateLiteralState ts = impl()->OpenTemplateLiteral(pos);
  bool is_valid = CheckTemplateEscapes(forbid_illegal_escapes);
  impl()->AddTemplateSpan(&ts, is_valid, false);
  Token::Value next;

  // If we open with a TEMPLATE_SPAN, we must scan the subsequent expression,
  // and repeat if the following token is a TEMPLATE_SPAN as well (in this
  // case, representing a TemplateMiddle).

  do {
    next = peek();

    int expr_pos = peek_position();
    ExpressionT expression = ParseExpressionCoverGrammar(true);
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    impl()->AddTemplateExpression(&ts, expression);

    if (peek() != Token::RBRACE) {
      impl()->ReportMessageAt(Scanner::Location(expr_pos, peek_position()),
                              MessageTemplate::kUnterminatedTemplateExpr);
      return impl()->NullExpression();
    }

    // If we didn't die parsing that expression, our next token should be a
    // TEMPLATE_SPAN or TEMPLATE_TAIL.
    next = scanner()->ScanTemplateContinuation();
    Next();
    pos = position();

    bool is_valid = CheckTemplateEscapes(forbid_illegal_escapes);
    impl()->AddTemplateSpan(&ts, is_valid, next == Token::TEMPLATE_TAIL);
  } while (next == Token::TEMPLATE_SPAN);

  RETURN_IF_PARSE_ERROR;
  DCHECK_EQ(next, Token::TEMPLATE_TAIL);
  // Once we've reached a TEMPLATE_TAIL, we can close the TemplateLiteral.
  return impl()->CloseTemplateLiteral(&ts, start, tag);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::CheckAndRewriteReferenceExpression(ExpressionT expression,
                                                     int beg_pos, int end_pos,
                                                     MessageTemplate message) {
  return CheckAndRewriteReferenceExpression(expression, beg_pos, end_pos,
                                            message, kReferenceError);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::CheckAndRewriteReferenceExpression(ExpressionT expression,
                                                     int beg_pos, int end_pos,
                                                     MessageTemplate message,
                                                     ParseErrorType type) {
  if (impl()->IsIdentifier(expression) && is_strict(language_mode()) &&
      impl()->IsEvalOrArguments(impl()->AsIdentifier(expression))) {
    ReportMessageAt(Scanner::Location(beg_pos, end_pos),
                    MessageTemplate::kStrictEvalArguments, kSyntaxError);
    return impl()->NullExpression();
  }
  if (expression->IsValidReferenceExpression()) {
    return expression;
  }
  if (expression->IsCall() && !expression->AsCall()->is_tagged_template()) {
    // If it is a call, make it a runtime error for legacy web compatibility.
    // Bug: https://bugs.chromium.org/p/v8/issues/detail?id=4480
    // Rewrite `expr' to `expr[throw ReferenceError]'.
    impl()->CountUsage(
        is_strict(language_mode())
            ? v8::Isolate::kAssigmentExpressionLHSIsCallInStrict
            : v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy);
    ExpressionT error = impl()->NewThrowReferenceError(message, beg_pos);
    return factory()->NewProperty(expression, error, beg_pos);
  }
  ReportMessageAt(Scanner::Location(beg_pos, end_pos), message, type);
  return impl()->NullExpression();
}

template <typename Impl>
bool ParserBase<Impl>::IsValidReferenceExpression(ExpressionT expression) {
  return IsAssignableIdentifier(expression) || expression->IsProperty();
}

template <typename Impl>
void ParserBase<Impl>::CheckDestructuringElement(ExpressionT expression,
                                                 int begin, int end) {
  if (!IsValidPattern(expression) && !expression->IsAssignment() &&
      !IsValidReferenceExpression(expression)) {
    classifier()->RecordAssignmentPatternError(
        Scanner::Location(begin, end),
        MessageTemplate::kInvalidDestructuringTarget);
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseV8Intrinsic() {
  // CallRuntime ::
  //   '%' Identifier Arguments

  int pos = peek_position();
  Consume(Token::MOD);
  // Allow "eval" or "arguments" for backward compatibility.
  IdentifierT name = ParseIdentifier(kAllowRestrictedIdentifiers);
  RETURN_IF_PARSE_ERROR;
  ExpressionClassifier classifier(this);
  if (peek() != Token::LPAREN) {
    impl()->ReportUnexpectedToken(peek());
    return impl()->NullExpression();
  }
  bool has_spread;
  ExpressionListT args(pointer_buffer());
  ParseArguments(&args, &has_spread);
  RETURN_IF_PARSE_ERROR;

  if (has_spread) {
    ReportMessageAt(Scanner::Location(pos, position()),
                    MessageTemplate::kIntrinsicWithSpread, kSyntaxError);
    return impl()->NullExpression();
  }

  return impl()->NewV8Intrinsic(name, args, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseDoExpression() {
  // AssignmentExpression ::
  //     do '{' StatementList '}'

  int pos = peek_position();
  Consume(Token::DO);
  BlockT block = ParseBlock(nullptr);
  RETURN_IF_PARSE_ERROR;
  return impl()->RewriteDoExpression(block, pos);
}

#undef RETURN_IF_PARSE_ERROR
#define RETURN_IF_PARSE_ERROR RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement)

template <typename Impl>
typename ParserBase<Impl>::LazyParsingResult
ParserBase<Impl>::ParseStatementList(StatementListT body,
                                     Token::Value end_token, bool may_abort) {
  // StatementList ::
  //   (StatementListItem)* <end_token>

  // Allocate a target stack to use for this set of source
  // elements. This way, all scripts and functions get their own
  // target stack thus avoiding illegal breaks and continues across
  // functions.
  typename Types::TargetScope target_scope(this);
  int count_statements = 0;

  DCHECK(!impl()->IsNull(body));
  bool directive_prologue = true;  // Parsing directive prologue.

  while (peek() != end_token) {
    if (directive_prologue && peek() != Token::STRING) {
      directive_prologue = false;
    }

    bool starts_with_identifier = peek() == Token::IDENTIFIER;
    Scanner::Location token_loc = scanner()->peek_location();
    StatementT stat = ParseStatementListItem();
    RETURN_IF_PARSE_ERROR_CUSTOM(Return, kLazyParsingComplete)

    if (impl()->IsNull(stat) || stat->IsEmptyStatement()) {
      directive_prologue = false;  // End of directive prologue.
      continue;
    }

    if (directive_prologue) {
      // The length of the token is used to distinguish between strings literals
      // that evaluate equal to directives but contain either escape sequences
      // (e.g., "use \x73trict") or line continuations (e.g., "use \(newline)
      // strict").
      if (impl()->IsUseStrictDirective(stat) &&
          token_loc.end_pos - token_loc.beg_pos == sizeof("use strict") + 1) {
        // Directive "use strict" (ES5 14.1).
        RaiseLanguageMode(LanguageMode::kStrict);
        if (!scope()->HasSimpleParameters()) {
          // TC39 deemed "use strict" directives to be an error when occurring
          // in the body of a function with non-simple parameter list, on
          // 29/7/2015. https://goo.gl/ueA7Ln
          impl()->ReportMessageAt(
              token_loc, MessageTemplate::kIllegalLanguageModeDirective,
              "use strict");
          return kLazyParsingComplete;
        }
      } else if (impl()->IsUseAsmDirective(stat) &&
                 token_loc.end_pos - token_loc.beg_pos ==
                     sizeof("use asm") + 1) {
        // Directive "use asm".
        impl()->SetAsmModule();
      } else if (impl()->IsStringLiteral(stat)) {
        // Possibly an unknown directive.
        // Should not change mode, but will increment usage counters
        // as appropriate. Ditto usages below.
        RaiseLanguageMode(LanguageMode::kSloppy);
      } else {
        // End of the directive prologue.
        directive_prologue = false;
        RaiseLanguageMode(LanguageMode::kSloppy);
      }
    } else {
      RaiseLanguageMode(LanguageMode::kSloppy);
    }

    // If we're allowed to abort, we will do so when we see a "long and
    // trivial" function. Our current definition of "long and trivial" is:
    // - over kLazyParseTrialLimit statements
    // - all starting with an identifier (i.e., no if, for, while, etc.)
    if (may_abort) {
      if (!starts_with_identifier) {
        may_abort = false;
      } else if (++count_statements > kLazyParseTrialLimit) {
        return kLazyParsingAborted;
      }
    }

    body->Add(stat, zone());
  }
  return kLazyParsingComplete;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseStatementListItem() {
  // ECMA 262 6th Edition
  // StatementListItem[Yield, Return] :
  //   Statement[?Yield, ?Return]
  //   Declaration[?Yield]
  //
  // Declaration[Yield] :
  //   HoistableDeclaration[?Yield]
  //   ClassDeclaration[?Yield]
  //   LexicalDeclaration[In, ?Yield]
  //
  // HoistableDeclaration[Yield, Default] :
  //   FunctionDeclaration[?Yield, ?Default]
  //   GeneratorDeclaration[?Yield, ?Default]
  //
  // LexicalDeclaration[In, Yield] :
  //   LetOrConst BindingList[?In, ?Yield] ;

  switch (peek()) {
    case Token::FUNCTION:
      return ParseHoistableDeclaration(nullptr, false);
    case Token::CLASS:
      Consume(Token::CLASS);
      return ParseClassDeclaration(nullptr, false);
    case Token::VAR:
    case Token::CONST:
      return ParseVariableStatement(kStatementListItem, nullptr);
    case Token::LET:
      if (IsNextLetKeyword()) {
        return ParseVariableStatement(kStatementListItem, nullptr);
      }
      break;
    case Token::ASYNC:
      if (PeekAhead() == Token::FUNCTION &&
          !scanner()->HasLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        return ParseAsyncFunctionDeclaration(nullptr, false);
      }
      break;
    default:
      break;
  }
  return ParseStatement(nullptr, nullptr, kAllowLabelledFunctionStatement);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseStatement(
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels,
    AllowLabelledFunctionStatement allow_function) {
  // Statement ::
  //   Block
  //   VariableStatement
  //   EmptyStatement
  //   ExpressionStatement
  //   IfStatement
  //   IterationStatement
  //   ContinueStatement
  //   BreakStatement
  //   ReturnStatement
  //   WithStatement
  //   LabelledStatement
  //   SwitchStatement
  //   ThrowStatement
  //   TryStatement
  //   DebuggerStatement

  // {own_labels} is always a subset of {labels}.
  DCHECK_IMPLIES(labels == nullptr, own_labels == nullptr);

  // Note: Since labels can only be used by 'break' and 'continue'
  // statements, which themselves are only valid within blocks,
  // iterations or 'switch' statements (i.e., BreakableStatements),
  // labels can be simply ignored in all other cases; except for
  // trivial labeled break statements 'label: break label' which is
  // parsed into an empty statement.
  switch (peek()) {
    case Token::LBRACE:
      return ParseBlock(labels);
    case Token::SEMICOLON:
      Next();
      return factory()->NewEmptyStatement(kNoSourcePosition);
    case Token::IF:
      return ParseIfStatement(labels);
    case Token::DO:
      return ParseDoWhileStatement(labels, own_labels);
    case Token::WHILE:
      return ParseWhileStatement(labels, own_labels);
    case Token::FOR:
      if (V8_UNLIKELY(is_async_function() && PeekAhead() == Token::AWAIT)) {
        return ParseForAwaitStatement(labels, own_labels);
      }
      return ParseForStatement(labels, own_labels);
    case Token::CONTINUE:
      return ParseContinueStatement();
    case Token::BREAK:
      return ParseBreakStatement(labels);
    case Token::RETURN:
      return ParseReturnStatement();
    case Token::THROW:
      return ParseThrowStatement();
    case Token::TRY: {
      // It is somewhat complicated to have labels on try-statements.
      // When breaking out of a try-finally statement, one must take
      // great care not to treat it as a fall-through. It is much easier
      // just to wrap the entire try-statement in a statement block and
      // put the labels there.
      if (labels == nullptr) return ParseTryStatement();
      BlockT result = factory()->NewBlock(1, false, labels);
      typename Types::Target target(this, result);
      StatementT statement = ParseTryStatement();
      RETURN_IF_PARSE_ERROR;
      result->statements()->Add(statement, zone());
      return result;
    }
    case Token::WITH:
      return ParseWithStatement(labels);
    case Token::SWITCH:
      return ParseSwitchStatement(labels);
    case Token::FUNCTION:
      // FunctionDeclaration only allowed as a StatementListItem, not in
      // an arbitrary Statement position. Exceptions such as
      // ES#sec-functiondeclarations-in-ifstatement-statement-clauses
      // are handled by calling ParseScopedStatement rather than
      // ParseStatement directly.
      impl()->ReportMessageAt(scanner()->peek_location(),
                              is_strict(language_mode())
                                  ? MessageTemplate::kStrictFunction
                                  : MessageTemplate::kSloppyFunction);
      return impl()->NullStatement();
    case Token::DEBUGGER:
      return ParseDebuggerStatement();
    case Token::VAR:
      return ParseVariableStatement(kStatement, nullptr);
    case Token::ASYNC:
      if (!scanner()->HasLineTerminatorAfterNext() &&
          PeekAhead() == Token::FUNCTION) {
        impl()->ReportMessageAt(
            scanner()->peek_location(),
            MessageTemplate::kAsyncFunctionInSingleStatementContext);
        return impl()->NullStatement();
      }
      V8_FALLTHROUGH;
    default:
      return ParseExpressionOrLabelledStatement(labels, own_labels,
                                                allow_function);
  }
}

template <typename Impl>
typename ParserBase<Impl>::BlockT ParserBase<Impl>::ParseBlock(
    ZonePtrList<const AstRawString>* labels) {
  // Block ::
  //   '{' StatementList '}'

  // Construct block expecting 16 statements.
  BlockT body = factory()->NewBlock(16, false, labels);

  // Parse the statements and collect escaping labels.
  Expect(Token::LBRACE);

  CheckStackOverflow();

  {
    BlockState block_state(zone(), &scope_);
    scope()->set_start_position(scanner()->location().beg_pos);
    typename Types::Target target(this, body);

    while (peek() != Token::RBRACE) {
      StatementT stat = ParseStatementListItem();
      RETURN_IF_PARSE_ERROR_CUSTOM(NullStatement);
      if (!impl()->IsNull(stat) && !stat->IsEmptyStatement()) {
        body->statements()->Add(stat, zone());
      }
    }

    Expect(Token::RBRACE);
    int end_pos = end_position();
    scope()->set_end_position(end_pos);
    impl()->RecordBlockSourceRange(body, end_pos);
    body->set_scope(scope()->FinalizeBlockScope());
  }
  return body;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseScopedStatement(
    ZonePtrList<const AstRawString>* labels) {
  if (is_strict(language_mode()) || peek() != Token::FUNCTION) {
    return ParseStatement(labels, nullptr);
  } else {
    // Make a block around the statement for a lexical binding
    // is introduced by a FunctionDeclaration.
    BlockState block_state(zone(), &scope_);
    scope()->set_start_position(scanner()->location().beg_pos);
    BlockT block = factory()->NewBlock(1, false);
    StatementT body = ParseFunctionDeclaration();
    block->statements()->Add(body, zone());
    scope()->set_end_position(end_position());
    block->set_scope(scope()->FinalizeBlockScope());
    return block;
  }
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseVariableStatement(
    VariableDeclarationContext var_context,
    ZonePtrList<const AstRawString>* names) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  // The scope of a var declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). Thus we can
  // transform a source-level var declaration into a (Function) Scope
  // declaration, and rewrite the source-level initialization into an assignment
  // statement. We use a block to collect multiple assignments.
  //
  // We mark the block as initializer block because we don't want the
  // rewriter to add a '.result' assignment to such a block (to get compliant
  // behavior for code such as print(eval('var x = 7')), and for cosmetic
  // reasons when pretty-printing. Also, unless an assignment (initialization)
  // is inside an initializer block, it is ignored.

  DeclarationParsingResult parsing_result;
  StatementT result =
      ParseVariableDeclarations(var_context, &parsing_result, names);
  ExpectSemicolon();
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseDebuggerStatement() {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as i a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  int pos = peek_position();
  Consume(Token::DEBUGGER);
  ExpectSemicolon();
  return factory()->NewDebuggerStatement(pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseExpressionOrLabelledStatement(
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels,
    AllowLabelledFunctionStatement allow_function) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement
  //
  // ExpressionStatement[Yield] :
  //   [lookahead notin {{, function, class, let [}] Expression[In, ?Yield] ;

  int pos = peek_position();

  switch (peek()) {
    case Token::FUNCTION:
    case Token::LBRACE:
      UNREACHABLE();  // Always handled by the callers.
    case Token::CLASS:
      ReportUnexpectedToken(Next());
      return impl()->NullStatement();
    case Token::LET: {
      Token::Value next_next = PeekAhead();
      // "let" followed by either "[", "{" or an identifier means a lexical
      // declaration, which should not appear here.
      // However, ASI may insert a line break before an identifier or a brace.
      if (next_next != Token::LBRACK &&
          ((next_next != Token::LBRACE && next_next != Token::IDENTIFIER) ||
           scanner_->HasLineTerminatorAfterNext())) {
        break;
      }
      impl()->ReportMessageAt(scanner()->peek_location(),
                              MessageTemplate::kUnexpectedLexicalDeclaration);
      return impl()->NullStatement();
    }
    default:
      break;
  }

  bool starts_with_identifier = peek_any_identifier();
  ExpressionT expr = ParseExpression();
  // TODO(verwaest): Remove once we have FailureExpression.
  RETURN_IF_PARSE_ERROR;
  if (peek() == Token::COLON && starts_with_identifier &&
      impl()->IsIdentifier(expr)) {
    // The whole expression was a single identifier, and not, e.g.,
    // something starting with an identifier or a parenthesized identifier.
    impl()->DeclareLabel(&labels, &own_labels,
                         impl()->AsIdentifierExpression(expr));
    Consume(Token::COLON);
    // ES#sec-labelled-function-declarations Labelled Function Declarations
    if (peek() == Token::FUNCTION && is_sloppy(language_mode()) &&
        allow_function == kAllowLabelledFunctionStatement) {
      return ParseFunctionDeclaration();
    }
    return ParseStatement(labels, own_labels, allow_function);
  }

  // If we have an extension, we allow a native function declaration.
  // A native function declaration starts with "native function" with
  // no line-terminator between the two words.
  if (extension_ != nullptr && peek() == Token::FUNCTION &&
      !scanner()->HasLineTerminatorBeforeNext() && impl()->IsNative(expr) &&
      !scanner()->literal_contains_escapes()) {
    return ParseNativeDeclaration();
  }

  // Parsed expression statement, followed by semicolon.
  ExpectSemicolon();
  return factory()->NewExpressionStatement(expr, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseIfStatement(
    ZonePtrList<const AstRawString>* labels) {
  // IfStatement ::
  //   'if' '(' Expression ')' Statement ('else' Statement)?

  int pos = peek_position();
  Consume(Token::IF);
  Expect(Token::LPAREN);
  ExpressionT condition = ParseExpression();
  Expect(Token::RPAREN);

  SourceRange then_range, else_range;
  StatementT then_statement = impl()->NullStatement();
  {
    SourceRangeScope range_scope(scanner(), &then_range);
    then_statement = ParseScopedStatement(labels);
  }

  StatementT else_statement = impl()->NullStatement();
  if (Check(Token::ELSE)) {
    else_statement = ParseScopedStatement(labels);
    else_range = SourceRange::ContinuationOf(then_range, end_position());
  } else {
    else_statement = factory()->NewEmptyStatement(kNoSourcePosition);
  }
  StatementT stmt =
      factory()->NewIfStatement(condition, then_statement, else_statement, pos);
  impl()->RecordIfStatementSourceRange(stmt, then_range, else_range);
  return stmt;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseContinueStatement() {
  // ContinueStatement ::
  //   'continue' Identifier? ';'

  int pos = peek_position();
  Consume(Token::CONTINUE);
  IdentifierT label = impl()->NullIdentifier();
  Token::Value tok = peek();
  if (!scanner()->HasLineTerminatorBeforeNext() &&
      !Token::IsAutoSemicolon(tok)) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    label = ParseIdentifier(kAllowRestrictedIdentifiers);
  }
  RETURN_IF_PARSE_ERROR;
  typename Types::IterationStatement target =
      impl()->LookupContinueTarget(label);
  if (impl()->IsNull(target)) {
    // Illegal continue statement.
    MessageTemplate message = MessageTemplate::kIllegalContinue;
    typename Types::BreakableStatement breakable_target =
        impl()->LookupBreakTarget(label);
    if (impl()->IsNull(label)) {
      message = MessageTemplate::kNoIterationStatement;
    } else if (impl()->IsNull(breakable_target)) {
      message = MessageTemplate::kUnknownLabel;
    }
    ReportMessage(message, label);
    return impl()->NullStatement();
  }
  ExpectSemicolon();
  StatementT stmt = factory()->NewContinueStatement(target, pos);
  impl()->RecordJumpStatementSourceRange(stmt, end_position());
  return stmt;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseBreakStatement(
    ZonePtrList<const AstRawString>* labels) {
  // BreakStatement ::
  //   'break' Identifier? ';'

  int pos = peek_position();
  Consume(Token::BREAK);
  IdentifierT label = impl()->NullIdentifier();
  Token::Value tok = peek();
  if (!scanner()->HasLineTerminatorBeforeNext() &&
      !Token::IsAutoSemicolon(tok)) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    label = ParseIdentifier(kAllowRestrictedIdentifiers);
  }
  // Parse labeled break statements that target themselves into
  // empty statements, e.g. 'l1: l2: l3: break l2;'
  if (!impl()->IsNull(label) && impl()->ContainsLabel(labels, label)) {
    ExpectSemicolon();
    return factory()->NewEmptyStatement(pos);
  }
  typename Types::BreakableStatement target = impl()->LookupBreakTarget(label);
  if (impl()->IsNull(target)) {
    // Illegal break statement.
    MessageTemplate message = MessageTemplate::kIllegalBreak;
    if (!impl()->IsNull(label)) {
      message = MessageTemplate::kUnknownLabel;
    }
    ReportMessage(message, label);
    return impl()->NullStatement();
  }
  ExpectSemicolon();
  StatementT stmt = factory()->NewBreakStatement(target, pos);
  impl()->RecordJumpStatementSourceRange(stmt, end_position());
  return stmt;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseReturnStatement() {
  // ReturnStatement ::
  //   'return' [no line terminator] Expression? ';'

  // Consume the return token. It is necessary to do that before
  // reporting any errors on it, because of the way errors are
  // reported (underlining).
  Consume(Token::RETURN);
  Scanner::Location loc = scanner()->location();

  switch (GetDeclarationScope()->scope_type()) {
    case SCRIPT_SCOPE:
    case EVAL_SCOPE:
    case MODULE_SCOPE:
      impl()->ReportMessageAt(loc, MessageTemplate::kIllegalReturn);
      return impl()->NullStatement();
    default:
      break;
  }

  Token::Value tok = peek();
  ExpressionT return_value = impl()->NullExpression();
  if (scanner()->HasLineTerminatorBeforeNext() || Token::IsAutoSemicolon(tok)) {
    if (IsDerivedConstructor(function_state_->kind())) {
      return_value = impl()->ThisExpression(loc.beg_pos);
    }
  } else {
    return_value = ParseExpression();
  }
  ExpectSemicolon();

  return_value = impl()->RewriteReturn(return_value, loc.beg_pos);
  int continuation_pos = end_position();
  StatementT stmt =
      BuildReturnStatement(return_value, loc.beg_pos, continuation_pos);
  impl()->RecordJumpStatementSourceRange(stmt, end_position());
  return stmt;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseWithStatement(
    ZonePtrList<const AstRawString>* labels) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement

  Consume(Token::WITH);
  int pos = position();

  if (is_strict(language_mode())) {
    ReportMessage(MessageTemplate::kStrictWith);
    return impl()->NullStatement();
  }

  Expect(Token::LPAREN);
  ExpressionT expr = ParseExpression();
  Expect(Token::RPAREN);

  Scope* with_scope = NewScope(WITH_SCOPE);
  StatementT body = impl()->NullStatement();
  {
    BlockState block_state(&scope_, with_scope);
    with_scope->set_start_position(scanner()->peek_location().beg_pos);
    body = ParseStatement(labels, nullptr);
    with_scope->set_end_position(end_position());
  }
  return factory()->NewWithStatement(with_scope, expr, body, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseDoWhileStatement(
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'
  auto loop =
      factory()->NewDoWhileStatement(labels, own_labels, peek_position());
  typename Types::Target target(this, loop);

  SourceRange body_range;
  StatementT body = impl()->NullStatement();

  Consume(Token::DO);

  CheckStackOverflow();
  {
    SourceRangeScope range_scope(scanner(), &body_range);
    body = ParseStatement(nullptr, nullptr);
  }
  Expect(Token::WHILE);
  Expect(Token::LPAREN);

  ExpressionT cond = ParseExpression();
  Expect(Token::RPAREN);

  // Allow do-statements to be terminated with and without
  // semi-colons. This allows code such as 'do;while(0)return' to
  // parse, which would not be the case if we had used the
  // ExpectSemicolon() functionality here.
  Check(Token::SEMICOLON);

  loop->Initialize(cond, body);
  impl()->RecordIterationStatementSourceRange(loop, body_range);

  return loop;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseWhileStatement(
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  auto loop = factory()->NewWhileStatement(labels, own_labels, peek_position());
  typename Types::Target target(this, loop);

  SourceRange body_range;
  StatementT body = impl()->NullStatement();

  Consume(Token::WHILE);
  Expect(Token::LPAREN);
  ExpressionT cond = ParseExpression();
  Expect(Token::RPAREN);
  {
    SourceRangeScope range_scope(scanner(), &body_range);
    body = ParseStatement(nullptr, nullptr);
  }

  loop->Initialize(cond, body);
  impl()->RecordIterationStatementSourceRange(loop, body_range);

  return loop;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseThrowStatement() {
  // ThrowStatement ::
  //   'throw' Expression ';'

  Consume(Token::THROW);
  int pos = position();
  if (scanner()->HasLineTerminatorBeforeNext()) {
    ReportMessage(MessageTemplate::kNewlineAfterThrow);
    return impl()->NullStatement();
  }
  ExpressionT exception = ParseExpression();
  ExpectSemicolon();

  StatementT stmt = impl()->NewThrowStatement(exception, pos);
  impl()->RecordThrowSourceRange(stmt, end_position());

  return stmt;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseSwitchStatement(
    ZonePtrList<const AstRawString>* labels) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'
  // CaseClause ::
  //   'case' Expression ':' StatementList
  //   'default' ':' StatementList

  int switch_pos = peek_position();

  Consume(Token::SWITCH);
  Expect(Token::LPAREN);
  ExpressionT tag = ParseExpression();
  Expect(Token::RPAREN);

  auto switch_statement =
      factory()->NewSwitchStatement(labels, tag, switch_pos);

  {
    BlockState cases_block_state(zone(), &scope_);
    scope()->set_start_position(switch_pos);
    scope()->SetNonlinear();
    typename Types::Target target(this, switch_statement);

    bool default_seen = false;
    Expect(Token::LBRACE);
    while (peek() != Token::RBRACE) {
      // An empty label indicates the default case.
      ExpressionT label = impl()->NullExpression();
      ScopedStatementListT statements(pointer_buffer());
      SourceRange clause_range;
      {
        SourceRangeScope range_scope(scanner(), &clause_range);
        if (Check(Token::CASE)) {
          label = ParseExpression();
        } else {
          Expect(Token::DEFAULT);
          RETURN_IF_PARSE_ERROR;
          if (default_seen) {
            ReportMessage(MessageTemplate::kMultipleDefaultsInSwitch);
            return impl()->NullStatement();
          }
          default_seen = true;
        }
        Expect(Token::COLON);
        while (peek() != Token::CASE && peek() != Token::DEFAULT &&
               peek() != Token::RBRACE) {
          StatementT stat = ParseStatementListItem();
          RETURN_IF_PARSE_ERROR;
          statements.Add(stat);
        }
      }
      auto clause = factory()->NewCaseClause(label, statements);
      impl()->RecordCaseClauseSourceRange(clause, clause_range);
      switch_statement->cases()->Add(clause, zone());
    }
    Expect(Token::RBRACE);

    int end_pos = end_position();
    scope()->set_end_position(end_pos);
    impl()->RecordSwitchStatementSourceRange(switch_statement, end_pos);
    Scope* switch_scope = scope()->FinalizeBlockScope();
    if (switch_scope != nullptr) {
      return impl()->RewriteSwitchStatement(switch_statement, switch_scope);
    }
    return switch_statement;
  }
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseTryStatement() {
  // TryStatement ::
  //   'try' Block Catch
  //   'try' Block Finally
  //   'try' Block Catch Finally
  //
  // Catch ::
  //   'catch' '(' Identifier ')' Block
  //
  // Finally ::
  //   'finally' Block

  Consume(Token::TRY);
  int pos = position();

  BlockT try_block = ParseBlock(nullptr);

  CatchInfo catch_info(this);

  if (peek() != Token::CATCH && peek() != Token::FINALLY) {
    ReportMessage(MessageTemplate::kNoCatchOrFinally);
    return impl()->NullStatement();
  }

  SourceRange catch_range, finally_range;

  BlockT catch_block = impl()->NullStatement();
  {
    SourceRangeScope catch_range_scope(scanner(), &catch_range);
    if (Check(Token::CATCH)) {
      bool has_binding;
      has_binding = Check(Token::LPAREN);

      if (has_binding) {
        catch_info.scope = NewScope(CATCH_SCOPE);
        catch_info.scope->set_start_position(scanner()->location().beg_pos);

        {
          BlockState catch_block_state(&scope_, catch_info.scope);

          catch_block = factory()->NewBlock(16, false);

          // Create a block scope to hold any lexical declarations created
          // as part of destructuring the catch parameter.
          {
            BlockState catch_variable_block_state(zone(), &scope_);
            scope()->set_start_position(scanner()->location().beg_pos);

            // This does not simply call ParsePrimaryExpression to avoid
            // ExpressionFromIdentifier from being called in the first
            // branch, which would introduce an unresolved symbol and mess
            // with arrow function names.
            if (peek_any_identifier()) {
              catch_info.name =
                  ParseIdentifier(kDontAllowRestrictedIdentifiers);
            } else {
              ExpressionClassifier pattern_classifier(this);
              catch_info.pattern = ParseBindingPattern();
            }

            Expect(Token::RPAREN);
            RETURN_IF_PARSE_ERROR;
            impl()->RewriteCatchPattern(&catch_info);
            if (!impl()->IsNull(catch_info.init_block)) {
              catch_block->statements()->Add(catch_info.init_block, zone());
            }

            catch_info.inner_block = ParseBlock(nullptr);
            catch_block->statements()->Add(catch_info.inner_block, zone());
            RETURN_IF_PARSE_ERROR;
            impl()->ValidateCatchBlock(catch_info);
            scope()->set_end_position(end_position());
            catch_block->set_scope(scope()->FinalizeBlockScope());
          }
        }

        catch_info.scope->set_end_position(end_position());
      } else {
        catch_block = ParseBlock(nullptr);
      }
    }
  }

  BlockT finally_block = impl()->NullStatement();
  DCHECK(has_error() || peek() == Token::FINALLY ||
         !impl()->IsNull(catch_block));
  {
    SourceRangeScope range_scope(scanner(), &finally_range);
    if (Check(Token::FINALLY)) {
      finally_block = ParseBlock(nullptr);
    }
  }

  RETURN_IF_PARSE_ERROR;
  return impl()->RewriteTryStatement(try_block, catch_block, catch_range,
                                     finally_block, finally_range, catch_info,
                                     pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseForStatement(
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels) {
  // Either a standard for loop
  //   for (<init>; <cond>; <next>) { ... }
  // or a for-each loop
  //   for (<each> of|in <iterable>) { ... }
  //
  // We parse a declaration/expression after the 'for (' and then read the first
  // expression/declaration before we know if this is a for or a for-each.

  int stmt_pos = peek_position();
  ForInfo for_info(this);

  Consume(Token::FOR);
  Expect(Token::LPAREN);

  if (peek() == Token::CONST || (peek() == Token::LET && IsNextLetKeyword())) {
    // The initializer contains lexical declarations,
    // so create an in-between scope.
    BlockState for_state(zone(), &scope_);
    scope()->set_start_position(scanner()->location().beg_pos);

    // Also record whether inner functions or evals are found inside
    // this loop, as this information is used to simplify the desugaring
    // if none are found.
    typename FunctionState::FunctionOrEvalRecordingScope recording_scope(
        function_state_);

    // Create an inner block scope which will be the parent scope of scopes
    // possibly created by ParseVariableDeclarations.
    Scope* inner_block_scope = NewScope(BLOCK_SCOPE);
    {
      BlockState inner_state(&scope_, inner_block_scope);
      ParseVariableDeclarations(kForStatement, &for_info.parsing_result,
                                nullptr);
    }
    DCHECK(IsLexicalVariableMode(for_info.parsing_result.descriptor.mode));
    for_info.position = scanner()->location().beg_pos;

    if (CheckInOrOf(&for_info.mode)) {
      scope()->set_is_hidden();
      return ParseForEachStatementWithDeclarations(
          stmt_pos, &for_info, labels, own_labels, inner_block_scope);
    }

    Expect(Token::SEMICOLON);

    RETURN_IF_PARSE_ERROR;
    StatementT init = impl()->BuildInitializationBlock(&for_info.parsing_result,
                                                       &for_info.bound_names);

    Scope* finalized = inner_block_scope->FinalizeBlockScope();
    // No variable declarations will have been created in inner_block_scope.
    DCHECK_NULL(finalized);
    USE(finalized);
    return ParseStandardForLoopWithLexicalDeclarations(
        stmt_pos, init, &for_info, labels, own_labels);
  }

  StatementT init = impl()->NullStatement();
  if (peek() == Token::VAR) {
    ParseVariableDeclarations(kForStatement, &for_info.parsing_result, nullptr);
    DCHECK_EQ(for_info.parsing_result.descriptor.mode, VariableMode::kVar);
    for_info.position = scanner()->location().beg_pos;

    if (CheckInOrOf(&for_info.mode)) {
      return ParseForEachStatementWithDeclarations(stmt_pos, &for_info, labels,
                                                   own_labels, nullptr);
    }

    RETURN_IF_PARSE_ERROR;
    init = impl()->BuildInitializationBlock(&for_info.parsing_result, nullptr);
  } else if (peek() != Token::SEMICOLON) {
    // The initializer does not contain declarations.
    int lhs_beg_pos = peek_position();
    ExpressionClassifier classifier(this);
    ExpressionT expression = ParseExpressionCoverGrammar(false);
    int lhs_end_pos = end_position();

    bool is_for_each = CheckInOrOf(&for_info.mode);
    // TODO(verwaest): Remove once we have FailureExpression.
    RETURN_IF_PARSE_ERROR;
    bool is_destructuring = is_for_each && (expression->IsArrayLiteral() ||
                                            expression->IsObjectLiteral());

    if (is_destructuring) {
      ValidateAssignmentPattern();
    } else {
      ValidateExpression();
    }

    if (is_for_each) {
      return ParseForEachStatementWithoutDeclarations(
          stmt_pos, expression, lhs_beg_pos, lhs_end_pos, &for_info, labels,
          own_labels);
    }
    // Initializer is just an expression.
    init = factory()->NewExpressionStatement(expression, lhs_beg_pos);
  }

  Expect(Token::SEMICOLON);

  // Standard 'for' loop, we have parsed the initializer at this point.
  ExpressionT cond = impl()->NullExpression();
  StatementT next = impl()->NullStatement();
  StatementT body = impl()->NullStatement();
  ForStatementT loop =
      ParseStandardForLoop(stmt_pos, labels, own_labels, &cond, &next, &body);
  RETURN_IF_PARSE_ERROR;
  loop->Initialize(init, cond, next, body);
  return loop;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseForEachStatementWithDeclarations(
    int stmt_pos, ForInfo* for_info, ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels, Scope* inner_block_scope) {
  // Just one declaration followed by in/of.
  if (for_info->parsing_result.declarations.size() != 1) {
    impl()->ReportMessageAt(for_info->parsing_result.bindings_loc,
                            MessageTemplate::kForInOfLoopMultiBindings,
                            ForEachStatement::VisitModeString(for_info->mode));
    return impl()->NullStatement();
  }
  if (for_info->parsing_result.first_initializer_loc.IsValid() &&
      (is_strict(language_mode()) ||
       for_info->mode == ForEachStatement::ITERATE ||
       IsLexicalVariableMode(for_info->parsing_result.descriptor.mode) ||
       !impl()->IsIdentifier(
           for_info->parsing_result.declarations[0].pattern))) {
    impl()->ReportMessageAt(for_info->parsing_result.first_initializer_loc,
                            MessageTemplate::kForInOfLoopInitializer,
                            ForEachStatement::VisitModeString(for_info->mode));
    return impl()->NullStatement();
  }

  // Reset the declaration_kind to ensure proper processing during declaration.
  for_info->parsing_result.descriptor.declaration_kind =
      DeclarationDescriptor::FOR_EACH;

  BlockT init_block = impl()->RewriteForVarInLegacy(*for_info);

  auto loop = factory()->NewForEachStatement(for_info->mode, labels, own_labels,
                                             stmt_pos);
  typename Types::Target target(this, loop);

  ExpressionT enumerable = impl()->NullExpression();
  if (for_info->mode == ForEachStatement::ITERATE) {
    ExpressionClassifier classifier(this);
    enumerable = ParseAssignmentExpression(true);
    ValidateExpression();
  } else {
    enumerable = ParseExpression();
  }

  Expect(Token::RPAREN);

  Scope* for_scope = nullptr;
  if (inner_block_scope != nullptr) {
    for_scope = inner_block_scope->outer_scope();
    DCHECK_EQ(for_scope, scope());
    inner_block_scope->set_start_position(scanner()->location().beg_pos);
  }

  ExpressionT each_variable = impl()->NullExpression();
  BlockT body_block = impl()->NullStatement();
  {
    BlockState block_state(
        &scope_, inner_block_scope != nullptr ? inner_block_scope : scope_);

    SourceRange body_range;
    StatementT body = impl()->NullStatement();
    {
      SourceRangeScope range_scope(scanner(), &body_range);
      body = ParseStatement(nullptr, nullptr);
    }
    impl()->RecordIterationStatementSourceRange(loop, body_range);

    RETURN_IF_PARSE_ERROR;
    impl()->DesugarBindingInForEachStatement(for_info, &body_block,
                                             &each_variable);
    RETURN_IF_PARSE_ERROR;
    body_block->statements()->Add(body, zone());

    if (inner_block_scope != nullptr) {
      inner_block_scope->set_end_position(end_position());
      body_block->set_scope(inner_block_scope->FinalizeBlockScope());
    }
  }

  RETURN_IF_PARSE_ERROR;
  StatementT final_loop = impl()->InitializeForEachStatement(
      loop, each_variable, enumerable, body_block);

  init_block = impl()->CreateForEachStatementTDZ(init_block, *for_info);

  if (for_scope != nullptr) {
    for_scope->set_end_position(end_position());
    for_scope = for_scope->FinalizeBlockScope();
  }

  // Parsed for-in loop w/ variable declarations.
  if (!impl()->IsNull(init_block)) {
    init_block->statements()->Add(final_loop, zone());
    init_block->set_scope(for_scope);
    return init_block;
  }

  DCHECK_NULL(for_scope);
  return final_loop;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseForEachStatementWithoutDeclarations(
    int stmt_pos, ExpressionT expression, int lhs_beg_pos, int lhs_end_pos,
    ForInfo* for_info, ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels) {
  // Initializer is reference followed by in/of.
  if (!expression->IsArrayLiteral() && !expression->IsObjectLiteral()) {
    expression = CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, lhs_end_pos, MessageTemplate::kInvalidLhsInFor,
        kSyntaxError);
    RETURN_IF_PARSE_ERROR;
  }

  auto loop = factory()->NewForEachStatement(for_info->mode, labels, own_labels,
                                             stmt_pos);
  typename Types::Target target(this, loop);

  ExpressionT enumerable = impl()->NullExpression();
  if (for_info->mode == ForEachStatement::ITERATE) {
    ExpressionClassifier classifier(this);
    enumerable = ParseAssignmentExpression(true);
    ValidateExpression();
  } else {
    enumerable = ParseExpression();
  }

  Expect(Token::RPAREN);

  StatementT body = impl()->NullStatement();
  SourceRange body_range;
  {
    SourceRangeScope range_scope(scanner(), &body_range);
    body = ParseStatement(nullptr, nullptr);
  }
  impl()->RecordIterationStatementSourceRange(loop, body_range);
  RETURN_IF_PARSE_ERROR;
  return impl()->InitializeForEachStatement(loop, expression, enumerable, body);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseStandardForLoopWithLexicalDeclarations(
    int stmt_pos, StatementT init, ForInfo* for_info,
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels) {
  bool ok = true;
  // The condition and the next statement of the for loop must be parsed
  // in a new scope.
  Scope* inner_scope = NewScope(BLOCK_SCOPE);
  ForStatementT loop = impl()->NullStatement();
  ExpressionT cond = impl()->NullExpression();
  StatementT next = impl()->NullStatement();
  StatementT body = impl()->NullStatement();
  {
    BlockState block_state(&scope_, inner_scope);
    scope()->set_start_position(scanner()->location().beg_pos);
    loop =
        ParseStandardForLoop(stmt_pos, labels, own_labels, &cond, &next, &body);
    RETURN_IF_PARSE_ERROR;
    scope()->set_end_position(end_position());
  }

  scope()->set_end_position(end_position());
  if (for_info->bound_names.length() > 0 &&
      function_state_->contains_function_or_eval()) {
    scope()->set_is_hidden();
    return impl()->DesugarLexicalBindingsInForStatement(
        loop, init, cond, next, body, inner_scope, *for_info, &ok);
  } else {
    inner_scope = inner_scope->FinalizeBlockScope();
    DCHECK_NULL(inner_scope);
    USE(inner_scope);
  }

  Scope* for_scope = scope()->FinalizeBlockScope();
  if (for_scope != nullptr) {
    // Rewrite a for statement of the form
    //   for (const x = i; c; n) b
    //
    // into
    //
    //   {
    //     const x = i;
    //     for (; c; n) b
    //   }
    //
    DCHECK(!impl()->IsNull(init));
    BlockT block = factory()->NewBlock(2, false);
    block->statements()->Add(init, zone());
    block->statements()->Add(loop, zone());
    block->set_scope(for_scope);
    loop->Initialize(impl()->NullStatement(), cond, next, body);
    return block;
  }

  loop->Initialize(init, cond, next, body);
  return loop;
}

template <typename Impl>
typename ParserBase<Impl>::ForStatementT ParserBase<Impl>::ParseStandardForLoop(
    int stmt_pos, ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels, ExpressionT* cond,
    StatementT* next, StatementT* body) {
  ForStatementT loop = factory()->NewForStatement(labels, own_labels, stmt_pos);
  typename Types::Target target(this, loop);

  if (peek() != Token::SEMICOLON) {
    *cond = ParseExpression();
    RETURN_IF_PARSE_ERROR
  }
  Expect(Token::SEMICOLON);
  RETURN_IF_PARSE_ERROR;

  if (peek() != Token::RPAREN) {
    ExpressionT exp = ParseExpression();
    RETURN_IF_PARSE_ERROR;
    *next = factory()->NewExpressionStatement(exp, exp->position());
  }
  Expect(Token::RPAREN);
  RETURN_IF_PARSE_ERROR;

  SourceRange body_range;
  {
    SourceRangeScope range_scope(scanner(), &body_range);
    *body = ParseStatement(nullptr, nullptr);
    RETURN_IF_PARSE_ERROR;
  }
  impl()->RecordIterationStatementSourceRange(loop, body_range);

  return loop;
}

template <typename Impl>
void ParserBase<Impl>::MarkLoopVariableAsAssigned(
    Scope* scope, Variable* var,
    typename DeclarationDescriptor::Kind declaration_kind) {
  if (!IsLexicalVariableMode(var->mode()) &&
      (!scope->is_function_scope() ||
       declaration_kind == DeclarationDescriptor::FOR_EACH)) {
    var->set_maybe_assigned();
  }
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseForAwaitStatement(
    ZonePtrList<const AstRawString>* labels,
    ZonePtrList<const AstRawString>* own_labels) {
  // for await '(' ForDeclaration of AssignmentExpression ')'
  DCHECK(is_async_function());

  int stmt_pos = peek_position();

  ForInfo for_info(this);
  for_info.mode = ForEachStatement::ITERATE;

  // Create an in-between scope for let-bound iteration variables.
  BlockState for_state(zone(), &scope_);
  Expect(Token::FOR);
  RETURN_IF_PARSE_ERROR;
  Expect(Token::AWAIT);
  RETURN_IF_PARSE_ERROR;
  Expect(Token::LPAREN);
  RETURN_IF_PARSE_ERROR;
  scope()->set_start_position(scanner()->location().beg_pos);
  scope()->set_is_hidden();

  auto loop = factory()->NewForOfStatement(labels, own_labels, stmt_pos);
  typename Types::Target target(this, loop);

  ExpressionT each_variable = impl()->NullExpression();

  bool has_declarations = false;
  Scope* inner_block_scope = NewScope(BLOCK_SCOPE);

  if (peek() == Token::VAR || peek() == Token::CONST ||
      (peek() == Token::LET && IsNextLetKeyword())) {
    // The initializer contains declarations
    // 'for' 'await' '(' ForDeclaration 'of' AssignmentExpression ')'
    //     Statement
    // 'for' 'await' '(' 'var' ForBinding 'of' AssignmentExpression ')'
    //     Statement
    has_declarations = true;

    {
      BlockState inner_state(&scope_, inner_block_scope);
      ParseVariableDeclarations(kForStatement, &for_info.parsing_result,
                                nullptr);
      RETURN_IF_PARSE_ERROR;
    }
    for_info.position = scanner()->location().beg_pos;

    // Only a single declaration is allowed in for-await-of loops
    if (for_info.parsing_result.declarations.size() != 1) {
      impl()->ReportMessageAt(for_info.parsing_result.bindings_loc,
                              MessageTemplate::kForInOfLoopMultiBindings,
                              "for-await-of");
      return impl()->NullStatement();
    }

    // for-await-of's declarations do not permit initializers.
    if (for_info.parsing_result.first_initializer_loc.IsValid()) {
      impl()->ReportMessageAt(for_info.parsing_result.first_initializer_loc,
                              MessageTemplate::kForInOfLoopInitializer,
                              "for-await-of");
      return impl()->NullStatement();
    }
  } else {
    // The initializer does not contain declarations.
    // 'for' 'await' '(' LeftHandSideExpression 'of' AssignmentExpression ')'
    //     Statement
    int lhs_beg_pos = peek_position();
    BlockState inner_state(&scope_, inner_block_scope);
    ExpressionClassifier classifier(this);
    ExpressionT lhs = each_variable = ParseLeftHandSideExpression();
    RETURN_IF_PARSE_ERROR;
    int lhs_end_pos = end_position();

    if (lhs->IsArrayLiteral() || lhs->IsObjectLiteral()) {
      ValidateAssignmentPattern();
      RETURN_IF_PARSE_ERROR;
    } else {
      ValidateExpression();
      RETURN_IF_PARSE_ERROR;
      each_variable = CheckAndRewriteReferenceExpression(
          lhs, lhs_beg_pos, lhs_end_pos, MessageTemplate::kInvalidLhsInFor,
          kSyntaxError);
      RETURN_IF_PARSE_ERROR;
    }
  }

  ExpectContextualKeyword(Token::OF);
  RETURN_IF_PARSE_ERROR;
  int each_keyword_pos = scanner()->location().beg_pos;

  const bool kAllowIn = true;
  ExpressionT iterable = impl()->NullExpression();

  {
    ExpressionClassifier classifier(this);
    iterable = ParseAssignmentExpression(kAllowIn);
    RETURN_IF_PARSE_ERROR;
    ValidateExpression();
    RETURN_IF_PARSE_ERROR;
  }

  Expect(Token::RPAREN);
  RETURN_IF_PARSE_ERROR;

  StatementT body = impl()->NullStatement();
  {
    BlockState block_state(&scope_, inner_block_scope);
    scope()->set_start_position(scanner()->location().beg_pos);

    SourceRange body_range;
    {
      SourceRangeScope range_scope(scanner(), &body_range);
      body = ParseStatement(nullptr, nullptr);
      RETURN_IF_PARSE_ERROR;
      scope()->set_end_position(end_position());
    }
    impl()->RecordIterationStatementSourceRange(loop, body_range);

    if (has_declarations) {
      BlockT body_block = impl()->NullStatement();
      impl()->DesugarBindingInForEachStatement(&for_info, &body_block,
                                               &each_variable);
      RETURN_IF_PARSE_ERROR;
      body_block->statements()->Add(body, zone());
      body_block->set_scope(scope()->FinalizeBlockScope());
      body = body_block;
    } else {
      Scope* block_scope = scope()->FinalizeBlockScope();
      DCHECK_NULL(block_scope);
      USE(block_scope);
    }
  }
  const bool finalize = true;
  StatementT final_loop = impl()->InitializeForOfStatement(
      loop, each_variable, iterable, body, finalize, IteratorType::kAsync,
      each_keyword_pos);

  if (!has_declarations) {
    Scope* for_scope = scope()->FinalizeBlockScope();
    DCHECK_NULL(for_scope);
    USE(for_scope);
    return final_loop;
  }

  BlockT init_block =
      impl()->CreateForEachStatementTDZ(impl()->NullStatement(), for_info);
  RETURN_IF_PARSE_ERROR;

  scope()->set_end_position(end_position());
  Scope* for_scope = scope()->FinalizeBlockScope();
  // Parsed for-in loop w/ variable declarations.
  if (!impl()->IsNull(init_block)) {
    init_block->statements()->Add(final_loop, zone());
    init_block->set_scope(for_scope);
    return init_block;
  }
  DCHECK_NULL(for_scope);
  return final_loop;
}

template <typename Impl>
void ParserBase<Impl>::ObjectLiteralChecker::CheckDuplicateProto(
    Token::Value property) {
  if (property == Token::SMI || property == Token::NUMBER) return;

  if (IsProto()) {
    if (has_seen_proto_) {
      this->parser()->classifier()->RecordExpressionError(
          this->scanner()->location(), MessageTemplate::kDuplicateProto);
      return;
    }
    has_seen_proto_ = true;
  }
}

template <typename Impl>
void ParserBase<Impl>::ClassLiteralChecker::CheckClassMethodName(
    Token::Value property, ParsePropertyKind type, ParseFunctionFlags flags,
    bool is_static) {
  DCHECK(type == ParsePropertyKind::kMethod || IsAccessor(type));

  if (property == Token::SMI || property == Token::NUMBER) return;

  if (is_static) {
    if (IsPrototype()) {
      this->parser()->ReportMessage(MessageTemplate::kStaticPrototype);
      return;
    }
  } else if (IsConstructor()) {
    if (flags != ParseFunctionFlag::kIsNormal || IsAccessor(type)) {
      MessageTemplate msg = (flags & ParseFunctionFlag::kIsGenerator) != 0
                                ? MessageTemplate::kConstructorIsGenerator
                                : (flags & ParseFunctionFlag::kIsAsync) != 0
                                      ? MessageTemplate::kConstructorIsAsync
                                      : MessageTemplate::kConstructorIsAccessor;
      this->parser()->ReportMessage(msg);
      return;
    }
    if (has_seen_constructor_) {
      this->parser()->ReportMessage(MessageTemplate::kDuplicateConstructor);
      return;
    }
    has_seen_constructor_ = true;
    return;
  }
}

template <typename Impl>
void ParserBase<Impl>::ClassLiteralChecker::CheckClassFieldName(
    bool is_static) {
  if (is_static && IsPrototype()) {
    this->parser()->ReportMessage(MessageTemplate::kStaticPrototype);
    return;
  }

  if (IsConstructor() || IsPrivateConstructor()) {
    this->parser()->ReportMessage(MessageTemplate::kConstructorClassField);
    return;
  }
}

#undef RETURN_IF_PARSE_ERROR
#undef RETURN_IF_PARSE_ERROR_CUSTOM
#undef RETURN_IF_PARSE_ERROR_VOID

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSER_BASE_H_
