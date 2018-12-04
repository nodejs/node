// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cctype>

#include "src/torque/earley-parser.h"
#include "src/torque/torque-parser.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(CurrentAst);

using TypeList = std::vector<TypeExpression*>;
using GenericParameters = std::vector<std::string>;

struct ExpressionWithSource {
  Expression* expression;
  std::string source;
};

struct TypeswitchCase {
  SourcePosition pos;
  base::Optional<std::string> name;
  TypeExpression* type;
  Statement* block;
};

enum class ParseResultHolderBase::TypeId {
  kStdString,
  kBool,
  kStdVectorOfString,
  kExpressionPtr,
  kLocationExpressionPtr,
  kStatementPtr,
  kDeclarationPtr,
  kTypeExpressionPtr,
  kLabelBlockPtr,
  kNameAndTypeExpression,
  kStdVectorOfNameAndTypeExpression,
  kIncrementDecrementOperator,
  kOptionalStdString,
  kStdVectorOfStatementPtr,
  kStdVectorOfDeclarationPtr,
  kStdVectorOfExpressionPtr,
  kExpressionWithSource,
  kParameterList,
  kRangeExpression,
  kOptionalRangeExpression,
  kTypeList,
  kOptionalTypeList,
  kLabelAndTypes,
  kStdVectorOfLabelAndTypes,
  kStdVectorOfLabelBlockPtr,
  kOptionalStatementPtr,
  kOptionalExpressionPtr,
  kTypeswitchCase,
  kStdVectorOfTypeswitchCase
};

template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<std::string>::id =
    ParseResultTypeId::kStdString;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<bool>::id =
    ParseResultTypeId::kBool;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<std::string>>::id =
        ParseResultTypeId::kStdVectorOfString;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Declaration*>::id =
    ParseResultTypeId::kDeclarationPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<TypeExpression*>::id =
        ParseResultTypeId::kTypeExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<LabelBlock*>::id =
    ParseResultTypeId::kLabelBlockPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Expression*>::id =
    ParseResultTypeId::kExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<LocationExpression*>::id =
        ParseResultTypeId::kLocationExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Statement*>::id =
    ParseResultTypeId::kStatementPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<NameAndTypeExpression>::id =
        ParseResultTypeId::kNameAndTypeExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<NameAndTypeExpression>>::id =
        ParseResultTypeId::kStdVectorOfNameAndTypeExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<IncrementDecrementOperator>::id =
        ParseResultTypeId::kIncrementDecrementOperator;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<std::string>>::id =
        ParseResultTypeId::kOptionalStdString;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<Statement*>>::id =
        ParseResultTypeId::kStdVectorOfStatementPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<Declaration*>>::id =
        ParseResultTypeId::kStdVectorOfDeclarationPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<Expression*>>::id =
        ParseResultTypeId::kStdVectorOfExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<ExpressionWithSource>::id =
        ParseResultTypeId::kExpressionWithSource;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<ParameterList>::id =
    ParseResultTypeId::kParameterList;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<RangeExpression>::id =
        ParseResultTypeId::kRangeExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<RangeExpression>>::id =
        ParseResultTypeId::kOptionalRangeExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<TypeList>::id =
    ParseResultTypeId::kTypeList;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<TypeList>>::id =
        ParseResultTypeId::kOptionalTypeList;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<LabelAndTypes>::id =
    ParseResultTypeId::kLabelAndTypes;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<LabelAndTypes>>::id =
        ParseResultTypeId::kStdVectorOfLabelAndTypes;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<LabelBlock*>>::id =
        ParseResultTypeId::kStdVectorOfLabelBlockPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<Statement*>>::id =
        ParseResultTypeId::kOptionalStatementPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<Expression*>>::id =
        ParseResultTypeId::kOptionalExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<TypeswitchCase>::id = ParseResultTypeId::kTypeswitchCase;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<TypeswitchCase>>::id =
        ParseResultTypeId::kStdVectorOfTypeswitchCase;

namespace {

base::Optional<ParseResult> AddGlobalDeclaration(
    ParseResultIterator* child_results) {
  auto declaration = child_results->NextAs<Declaration*>();
  CurrentAst::Get().declarations().push_back(declaration);
  return base::nullopt;
}

template <class T, class... Args>
T* MakeNode(Args... args) {
  return CurrentAst::Get().AddNode(std::unique_ptr<T>(
      new T(CurrentSourcePosition::Get(), std::move(args)...)));
}

void LintGenericParameters(const GenericParameters& parameters) {
  for (const std::string& parameter : parameters) {
    if (!IsUpperCamelCase(parameter)) {
      NamingConventionError("Generic parameter", parameter, "UpperCamelCase");
    }
  }
}

void CheckNotDeferredStatement(Statement* statement) {
  if (BlockStatement* block = BlockStatement::DynamicCast(statement)) {
    if (block->deferred) {
      LintError(
          "cannot use deferred with a statement block here, it will have no "
          "effect");
    }
  }
}

Expression* MakeCall(const std::string& callee, bool is_operator,
                     const std::vector<TypeExpression*>& generic_arguments,
                     const std::vector<Expression*>& arguments,
                     const std::vector<Statement*>& otherwise) {
  std::vector<std::string> labels;

  // All IdentifierExpressions are treated as label names and can be directly
  // used as labels identifiers. All other statements in a call's otherwise
  // must create intermediate Labels for the otherwise's statement code.
  size_t label_id = 0;
  std::vector<LabelBlock*> temp_labels;
  for (auto* statement : otherwise) {
    if (auto* e = ExpressionStatement::DynamicCast(statement)) {
      if (auto* id = IdentifierExpression::DynamicCast(e->expression)) {
        if (id->generic_arguments.size() != 0) {
          ReportError("An otherwise label cannot have generic parameters");
        }
        labels.push_back(id->name);
        continue;
      }
    }
    auto label_name = std::string("_label") + std::to_string(label_id++);
    labels.push_back(label_name);
    auto* label_block =
        MakeNode<LabelBlock>(label_name, ParameterList::Empty(), statement);
    temp_labels.push_back(label_block);
  }

  // Create nested try-label expression for all of the temporary Labels that
  // were created.
  Expression* result = MakeNode<CallExpression>(
      callee, false, generic_arguments, arguments, labels);
  for (auto* label : temp_labels) {
    result = MakeNode<TryLabelExpression>(result, label);
  }
  return result;
}

base::Optional<ParseResult> MakeCall(ParseResultIterator* child_results) {
  auto callee = child_results->NextAs<std::string>();
  auto generic_args = child_results->NextAs<TypeList>();
  auto args = child_results->NextAs<std::vector<Expression*>>();
  auto otherwise = child_results->NextAs<std::vector<Statement*>>();
  return ParseResult{MakeCall(callee, false, generic_args, args, otherwise)};
}

base::Optional<ParseResult> MakeBinaryOperator(
    ParseResultIterator* child_results) {
  auto left = child_results->NextAs<Expression*>();
  auto op = child_results->NextAs<std::string>();
  auto right = child_results->NextAs<Expression*>();
  return ParseResult{MakeCall(op, true, TypeList{},
                              std::vector<Expression*>{left, right},
                              std::vector<Statement*>{})};
}

base::Optional<ParseResult> MakeUnaryOperator(
    ParseResultIterator* child_results) {
  auto op = child_results->NextAs<std::string>();
  auto e = child_results->NextAs<Expression*>();
  return ParseResult{MakeCall(op, true, TypeList{}, std::vector<Expression*>{e},
                              std::vector<Statement*>{})};
}

template <bool has_varargs>
base::Optional<ParseResult> MakeParameterListFromTypes(
    ParseResultIterator* child_results) {
  auto types = child_results->NextAs<TypeList>();
  ParameterList result;
  result.types = std::move(types);
  result.has_varargs = has_varargs;
  return ParseResult{std::move(result)};
}
template <bool has_varargs>
base::Optional<ParseResult> MakeParameterListFromNameAndTypeList(
    ParseResultIterator* child_results) {
  auto params = child_results->NextAs<std::vector<NameAndTypeExpression>>();
  std::string arguments_variable = "";
  if (child_results->HasNext()) {
    arguments_variable = child_results->NextAs<std::string>();
  }
  ParameterList result;
  for (NameAndTypeExpression& pair : params) {
    if (!IsLowerCamelCase(pair.name)) {
      NamingConventionError("Parameter", pair.name, "lowerCamelCase");
    }

    result.names.push_back(std::move(pair.name));
    result.types.push_back(pair.type);
  }
  result.has_varargs = has_varargs;
  result.arguments_variable = arguments_variable;
  return ParseResult{std::move(result)};
}

base::Optional<ParseResult> MakeAssertStatement(
    ParseResultIterator* child_results) {
  auto kind = child_results->NextAs<std::string>();
  auto expr_with_source = child_results->NextAs<ExpressionWithSource>();
  DCHECK(kind == "assert" || kind == "check");
  Statement* result = MakeNode<AssertStatement>(
      kind == "assert", expr_with_source.expression, expr_with_source.source);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeDebugStatement(
    ParseResultIterator* child_results) {
  auto kind = child_results->NextAs<std::string>();
  DCHECK(kind == "unreachable" || kind == "debug");
  Statement* result = MakeNode<DebugStatement>(kind, kind == "unreachable");
  return ParseResult{result};
}

base::Optional<ParseResult> MakeVoidType(ParseResultIterator* child_results) {
  TypeExpression* result = MakeNode<BasicTypeExpression>(false, "void");
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternalMacro(
    ParseResultIterator* child_results) {
  auto operator_name = child_results->NextAs<base::Optional<std::string>>();
  auto name = child_results->NextAs<std::string>();
  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();
  MacroDeclaration* macro = MakeNode<ExternalMacroDeclaration>(
      name, operator_name, args, return_type, labels);
  Declaration* result;
  if (generic_parameters.empty()) {
    result = MakeNode<StandardDeclaration>(macro, nullptr);
  } else {
    result = MakeNode<GenericDeclaration>(macro, generic_parameters);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTorqueMacroDeclaration(
    ParseResultIterator* child_results) {
  auto operator_name = child_results->NextAs<base::Optional<std::string>>();
  auto name = child_results->NextAs<std::string>();
  if (!IsUpperCamelCase(name)) {
    NamingConventionError("Macro", name, "UpperCamelCase");
  }

  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();
  auto body = child_results->NextAs<base::Optional<Statement*>>();
  MacroDeclaration* macro = MakeNode<TorqueMacroDeclaration>(
      name, operator_name, args, return_type, labels);
  Declaration* result;
  if (generic_parameters.empty()) {
    if (!body) ReportError("A non-generic declaration needs a body.");
    result = MakeNode<StandardDeclaration>(macro, *body);
  } else {
    result = MakeNode<GenericDeclaration>(macro, generic_parameters, body);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTorqueBuiltinDeclaration(
    ParseResultIterator* child_results) {
  auto javascript_linkage = child_results->NextAs<bool>();
  auto name = child_results->NextAs<std::string>();
  if (!IsUpperCamelCase(name)) {
    NamingConventionError("Builtin", name, "UpperCamelCase");
  }

  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto body = child_results->NextAs<base::Optional<Statement*>>();
  BuiltinDeclaration* builtin = MakeNode<TorqueBuiltinDeclaration>(
      javascript_linkage, name, args, return_type);
  Declaration* result;
  if (generic_parameters.empty()) {
    if (!body) ReportError("A non-generic declaration needs a body.");
    result = MakeNode<StandardDeclaration>(builtin, *body);
  } else {
    result = MakeNode<GenericDeclaration>(builtin, generic_parameters, body);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeConstDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  if (!IsValidModuleConstName(name)) {
    NamingConventionError("Constant", name, "kUpperCamelCase");
  }

  auto type = child_results->NextAs<TypeExpression*>();
  auto expression = child_results->NextAs<Expression*>();
  Declaration* result =
      MakeNode<ConstDeclaration>(std::move(name), type, expression);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternConstDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto type = child_results->NextAs<TypeExpression*>();
  auto literal = child_results->NextAs<std::string>();
  Declaration* result = MakeNode<ExternConstDeclaration>(std::move(name), type,
                                                         std::move(literal));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTypeAliasDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto type = child_results->NextAs<TypeExpression*>();
  Declaration* result = MakeNode<TypeAliasDeclaration>(std::move(name), type);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTypeDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  if (!IsValidTypeName(name)) {
    NamingConventionError("Type", name, "UpperCamelCase");
  }
  auto extends = child_results->NextAs<base::Optional<std::string>>();
  auto generates = child_results->NextAs<base::Optional<std::string>>();
  auto constexpr_generates =
      child_results->NextAs<base::Optional<std::string>>();
  Declaration* result = MakeNode<TypeDeclaration>(
      std::move(name), std::move(extends), std::move(generates),
      std::move(constexpr_generates));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExplicitModuleDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  if (!IsSnakeCase(name)) {
    NamingConventionError("Module", name, "snake_case");
  }
  auto declarations = child_results->NextAs<std::vector<Declaration*>>();
  Declaration* result = MakeNode<ExplicitModuleDeclaration>(
      std::move(name), std::move(declarations));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeSpecializationDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto generic_parameters =
      child_results->NextAs<std::vector<TypeExpression*>>();
  auto parameters = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();
  auto body = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(body);
  Declaration* result = MakeNode<SpecializationDeclaration>(
      std::move(name), std::move(generic_parameters), std::move(parameters),
      return_type, std::move(labels), body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeStructDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto fields = child_results->NextAs<std::vector<NameAndTypeExpression>>();
  Declaration* result =
      MakeNode<StructDeclaration>(std::move(name), std::move(fields));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternalBuiltin(
    ParseResultIterator* child_results) {
  auto js_linkage = child_results->NextAs<bool>();
  auto name = child_results->NextAs<std::string>();
  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  BuiltinDeclaration* builtin =
      MakeNode<ExternalBuiltinDeclaration>(js_linkage, name, args, return_type);
  Declaration* result;
  if (generic_parameters.empty()) {
    result = MakeNode<StandardDeclaration>(builtin, nullptr);
  } else {
    result = MakeNode<GenericDeclaration>(builtin, generic_parameters);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternalRuntime(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  ExternalRuntimeDeclaration* runtime =
      MakeNode<ExternalRuntimeDeclaration>(name, args, return_type);
  Declaration* result = MakeNode<StandardDeclaration>(runtime, nullptr);
  return ParseResult{result};
}

base::Optional<ParseResult> StringLiteralUnquoteAction(
    ParseResultIterator* child_results) {
  return ParseResult{
      StringLiteralUnquote(child_results->NextAs<std::string>())};
}

base::Optional<ParseResult> MakeBasicTypeExpression(
    ParseResultIterator* child_results) {
  auto is_constexpr = child_results->NextAs<bool>();
  auto name = child_results->NextAs<std::string>();
  TypeExpression* result =
      MakeNode<BasicTypeExpression>(is_constexpr, std::move(name));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeFunctionTypeExpression(
    ParseResultIterator* child_results) {
  auto parameters = child_results->NextAs<std::vector<TypeExpression*>>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  TypeExpression* result =
      MakeNode<FunctionTypeExpression>(std::move(parameters), return_type);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeUnionTypeExpression(
    ParseResultIterator* child_results) {
  auto a = child_results->NextAs<TypeExpression*>();
  auto b = child_results->NextAs<TypeExpression*>();
  TypeExpression* result = MakeNode<UnionTypeExpression>(a, b);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExpressionStatement(
    ParseResultIterator* child_results) {
  auto expression = child_results->NextAs<Expression*>();
  Statement* result = MakeNode<ExpressionStatement>(expression);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeIfStatement(
    ParseResultIterator* child_results) {
  auto is_constexpr = child_results->NextAs<bool>();
  auto condition = child_results->NextAs<Expression*>();
  auto if_true = child_results->NextAs<Statement*>();
  auto if_false = child_results->NextAs<base::Optional<Statement*>>();

  if (if_false && !(BlockStatement::DynamicCast(if_true) &&
                    (BlockStatement::DynamicCast(*if_false) ||
                     IfStatement::DynamicCast(*if_false)))) {
    ReportError("if-else statements require curly braces");
  }

  Statement* result =
      MakeNode<IfStatement>(is_constexpr, condition, if_true, if_false);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTypeswitchStatement(
    ParseResultIterator* child_results) {
  auto expression = child_results->NextAs<Expression*>();
  auto cases = child_results->NextAs<std::vector<TypeswitchCase>>();
  CurrentSourcePosition::Scope current_source_position(
      child_results->matched_input().pos);

  // typeswitch (expression) case (x1 : T1) {
  //   ...b1
  // } case (x2 : T2) {
  //   ...b2
  // } case (x3 : T3) {
  //   ...b3
  // }
  //
  // desugars to
  //
  // {
  //   const _value = expression;
  //   try {
  //     const x1 : T1 = cast<T1>(_value) otherwise _NextCase;
  //     ...b1
  //   } label _NextCase {
  //     try {
  //       const x2 : T2 = cast<T2>(%assume_impossible<T1>(_value));
  //       ...b2
  //     } label _NextCase {
  //       const x3 : T3 = %assume_impossible<T1|T2>(_value);
  //       ...b3
  //     }
  //   }
  // }

  BlockStatement* current_block = MakeNode<BlockStatement>();
  Statement* result = current_block;
  {
    CurrentSourcePosition::Scope current_source_position(expression->pos);
    current_block->statements.push_back(MakeNode<VarDeclarationStatement>(
        true, "_value", base::nullopt, expression));
  }

  TypeExpression* accumulated_types;
  for (size_t i = 0; i < cases.size(); ++i) {
    CurrentSourcePosition::Scope current_source_position(cases[i].pos);
    Expression* value = MakeNode<IdentifierExpression>("_value");
    if (i >= 1) {
      value =
          MakeNode<AssumeTypeImpossibleExpression>(accumulated_types, value);
    }
    BlockStatement* case_block;
    if (i < cases.size() - 1) {
      value =
          MakeCall("Cast", false, std::vector<TypeExpression*>{cases[i].type},
                   std::vector<Expression*>{value},
                   std::vector<Statement*>{MakeNode<ExpressionStatement>(
                       MakeNode<IdentifierExpression>("_NextCase"))});
      case_block = MakeNode<BlockStatement>();
    } else {
      case_block = current_block;
    }
    std::string name = "_case_value";
    if (cases[i].name) name = *cases[i].name;
    case_block->statements.push_back(
        MakeNode<VarDeclarationStatement>(true, name, cases[i].type, value));
    case_block->statements.push_back(cases[i].block);
    if (i < cases.size() - 1) {
      BlockStatement* next_block = MakeNode<BlockStatement>();
      current_block->statements.push_back(
          MakeNode<ExpressionStatement>(MakeNode<TryLabelExpression>(
              MakeNode<StatementExpression>(case_block),
              MakeNode<LabelBlock>("_NextCase", ParameterList::Empty(),
                                   next_block))));
      current_block = next_block;
    }
    accumulated_types =
        i > 0 ? MakeNode<UnionTypeExpression>(accumulated_types, cases[i].type)
              : cases[i].type;
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTypeswitchCase(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<base::Optional<std::string>>();
  auto type = child_results->NextAs<TypeExpression*>();
  auto block = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(block);
  return ParseResult{TypeswitchCase{child_results->matched_input().pos,
                                    std::move(name), type, block}};
}

base::Optional<ParseResult> MakeWhileStatement(
    ParseResultIterator* child_results) {
  auto condition = child_results->NextAs<Expression*>();
  auto body = child_results->NextAs<Statement*>();
  Statement* result = MakeNode<WhileStatement>(condition, body);
  CheckNotDeferredStatement(result);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeReturnStatement(
    ParseResultIterator* child_results) {
  auto value = child_results->NextAs<base::Optional<Expression*>>();
  Statement* result = MakeNode<ReturnStatement>(value);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTailCallStatement(
    ParseResultIterator* child_results) {
  auto value = child_results->NextAs<Expression*>();
  Statement* result = MakeNode<TailCallStatement>(CallExpression::cast(value));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeVarDeclarationStatement(
    ParseResultIterator* child_results) {
  auto kind = child_results->NextAs<std::string>();
  bool const_qualified = kind == "const";
  if (!const_qualified) DCHECK_EQ("let", kind);
  auto name = child_results->NextAs<std::string>();
  if (!IsLowerCamelCase(name)) {
    NamingConventionError("Variable", name, "lowerCamelCase");
  }

  auto type = child_results->NextAs<TypeExpression*>();
  base::Optional<Expression*> initializer;
  if (child_results->HasNext())
    initializer = child_results->NextAs<Expression*>();
  Statement* result = MakeNode<VarDeclarationStatement>(
      const_qualified, std::move(name), type, initializer);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeBreakStatement(
    ParseResultIterator* child_results) {
  Statement* result = MakeNode<BreakStatement>();
  return ParseResult{result};
}

base::Optional<ParseResult> MakeContinueStatement(
    ParseResultIterator* child_results) {
  Statement* result = MakeNode<ContinueStatement>();
  return ParseResult{result};
}

base::Optional<ParseResult> MakeGotoStatement(
    ParseResultIterator* child_results) {
  auto label = child_results->NextAs<std::string>();
  auto arguments = child_results->NextAs<std::vector<Expression*>>();
  Statement* result =
      MakeNode<GotoStatement>(std::move(label), std::move(arguments));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeBlockStatement(
    ParseResultIterator* child_results) {
  auto deferred = child_results->NextAs<bool>();
  auto statements = child_results->NextAs<std::vector<Statement*>>();
  Statement* result = MakeNode<BlockStatement>(deferred, std::move(statements));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTryLabelExpression(
    ParseResultIterator* child_results) {
  auto try_block = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(try_block);
  Statement* result = try_block;
  auto label_blocks = child_results->NextAs<std::vector<LabelBlock*>>();
  for (auto block : label_blocks) {
    result = MakeNode<ExpressionStatement>(MakeNode<TryLabelExpression>(
        MakeNode<StatementExpression>(result), block));
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeForOfLoopStatement(
    ParseResultIterator* child_results) {
  auto var_decl = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(var_decl);
  auto iterable = child_results->NextAs<Expression*>();
  auto range = child_results->NextAs<base::Optional<RangeExpression>>();
  auto body = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(body);
  Statement* result =
      MakeNode<ForOfLoopStatement>(var_decl, iterable, range, body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeForLoopStatement(
    ParseResultIterator* child_results) {
  auto var_decl = child_results->NextAs<base::Optional<Statement*>>();
  auto test = child_results->NextAs<base::Optional<Expression*>>();
  auto action = child_results->NextAs<base::Optional<Expression*>>();
  auto body = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(body);
  Statement* result = MakeNode<ForLoopStatement>(var_decl, test, action, body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeLabelBlock(ParseResultIterator* child_results) {
  auto label = child_results->NextAs<std::string>();
  if (!IsUpperCamelCase(label)) {
    NamingConventionError("Label", label, "UpperCamelCase");
  }
  auto parameters = child_results->NextAs<ParameterList>();
  auto body = child_results->NextAs<Statement*>();
  LabelBlock* result =
      MakeNode<LabelBlock>(std::move(label), std::move(parameters), body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeRangeExpression(
    ParseResultIterator* child_results) {
  auto begin = child_results->NextAs<base::Optional<Expression*>>();
  auto end = child_results->NextAs<base::Optional<Expression*>>();
  RangeExpression result = {begin, end};
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExpressionWithSource(
    ParseResultIterator* child_results) {
  auto e = child_results->NextAs<Expression*>();
  return ParseResult{
      ExpressionWithSource{e, child_results->matched_input().ToString()}};
}

base::Optional<ParseResult> MakeIdentifierExpression(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto generic_arguments =
      child_results->NextAs<std::vector<TypeExpression*>>();
  LocationExpression* result = MakeNode<IdentifierExpression>(
      std::move(name), std::move(generic_arguments));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeFieldAccessExpression(
    ParseResultIterator* child_results) {
  auto object = child_results->NextAs<Expression*>();
  auto field = child_results->NextAs<std::string>();
  LocationExpression* result =
      MakeNode<FieldAccessExpression>(object, std::move(field));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeElementAccessExpression(
    ParseResultIterator* child_results) {
  auto object = child_results->NextAs<Expression*>();
  auto field = child_results->NextAs<Expression*>();
  LocationExpression* result = MakeNode<ElementAccessExpression>(object, field);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeStructExpression(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto expressions = child_results->NextAs<std::vector<Expression*>>();
  Expression* result =
      MakeNode<StructExpression>(std::move(name), std::move(expressions));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeAssignmentExpression(
    ParseResultIterator* child_results) {
  auto location = child_results->NextAs<LocationExpression*>();
  auto op = child_results->NextAs<base::Optional<std::string>>();
  auto value = child_results->NextAs<Expression*>();
  Expression* result =
      MakeNode<AssignmentExpression>(location, std::move(op), value);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeNumberLiteralExpression(
    ParseResultIterator* child_results) {
  auto number = child_results->NextAs<std::string>();
  Expression* result = MakeNode<NumberLiteralExpression>(std::move(number));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeStringLiteralExpression(
    ParseResultIterator* child_results) {
  auto literal = child_results->NextAs<std::string>();
  Expression* result = MakeNode<StringLiteralExpression>(std::move(literal));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeIncrementDecrementExpressionPostfix(
    ParseResultIterator* child_results) {
  auto location = child_results->NextAs<LocationExpression*>();
  auto op = child_results->NextAs<IncrementDecrementOperator>();
  Expression* result =
      MakeNode<IncrementDecrementExpression>(location, op, true);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeIncrementDecrementExpressionPrefix(
    ParseResultIterator* child_results) {
  auto op = child_results->NextAs<IncrementDecrementOperator>();
  auto location = child_results->NextAs<LocationExpression*>();
  Expression* result =
      MakeNode<IncrementDecrementExpression>(location, op, false);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeLogicalOrExpression(
    ParseResultIterator* child_results) {
  auto left = child_results->NextAs<Expression*>();
  auto right = child_results->NextAs<Expression*>();
  Expression* result = MakeNode<LogicalOrExpression>(left, right);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeLogicalAndExpression(
    ParseResultIterator* child_results) {
  auto left = child_results->NextAs<Expression*>();
  auto right = child_results->NextAs<Expression*>();
  Expression* result = MakeNode<LogicalAndExpression>(left, right);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeConditionalExpression(
    ParseResultIterator* child_results) {
  auto condition = child_results->NextAs<Expression*>();
  auto if_true = child_results->NextAs<Expression*>();
  auto if_false = child_results->NextAs<Expression*>();
  Expression* result =
      MakeNode<ConditionalExpression>(condition, if_true, if_false);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeLabelAndTypes(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  if (!IsUpperCamelCase(name)) {
    NamingConventionError("Label", name, "UpperCamelCase");
  }
  auto types = child_results->NextAs<std::vector<TypeExpression*>>();
  return ParseResult{LabelAndTypes{std::move(name), std::move(types)}};
}

base::Optional<ParseResult> MakeNameAndType(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  auto type = child_results->NextAs<TypeExpression*>();
  return ParseResult{NameAndTypeExpression{std::move(name), type}};
}

base::Optional<ParseResult> ExtractAssignmentOperator(
    ParseResultIterator* child_results) {
  auto op = child_results->NextAs<std::string>();
  base::Optional<std::string> result = std::string(op.begin(), op.end() - 1);
  return ParseResult(std::move(result));
}

struct TorqueGrammar : Grammar {
  static bool MatchWhitespace(InputPosition* pos) {
    while (true) {
      if (MatchChar(std::isspace, pos)) continue;
      if (MatchString("//", pos)) {
        while (MatchChar([](char c) { return c != '\n'; }, pos)) {
        }
        continue;
      }
      return true;
    }
  }

  static bool MatchIdentifier(InputPosition* pos) {
    if (!MatchChar(std::isalpha, pos)) return false;
    while (MatchChar(std::isalnum, pos) || MatchString("_", pos)) {
    }
    return true;
  }

  static bool MatchStringLiteral(InputPosition* pos) {
    InputPosition current = *pos;
    if (MatchString("\"", &current)) {
      while (
          (MatchString("\\", &current) && MatchAnyChar(&current)) ||
          MatchChar([](char c) { return c != '"' && c != '\n'; }, &current)) {
      }
      if (MatchString("\"", &current)) {
        *pos = current;
        return true;
      }
    }
    current = *pos;
    if (MatchString("'", &current)) {
      while (
          (MatchString("\\", &current) && MatchAnyChar(&current)) ||
          MatchChar([](char c) { return c != '\'' && c != '\n'; }, &current)) {
      }
      if (MatchString("'", &current)) {
        *pos = current;
        return true;
      }
    }
    return false;
  }

  static bool MatchHexLiteral(InputPosition* pos) {
    InputPosition current = *pos;
    MatchString("-", &current);
    if (MatchString("0x", &current) && MatchChar(std::isxdigit, &current)) {
      while (MatchChar(std::isxdigit, &current)) {
      }
      *pos = current;
      return true;
    }
    return false;
  }

  static bool MatchDecimalLiteral(InputPosition* pos) {
    InputPosition current = *pos;
    bool found_digit = false;
    MatchString("-", &current);
    while (MatchChar(std::isdigit, &current)) found_digit = true;
    MatchString(".", &current);
    while (MatchChar(std::isdigit, &current)) found_digit = true;
    if (!found_digit) return false;
    *pos = current;
    if ((MatchString("e", &current) || MatchString("E", &current)) &&
        (MatchString("+", &current) || MatchString("-", &current) || true) &&
        MatchChar(std::isdigit, &current)) {
      while (MatchChar(std::isdigit, &current)) {
      }
      *pos = current;
      return true;
    }
    return true;
  }

  TorqueGrammar() : Grammar(&file) { SetWhitespace(MatchWhitespace); }

  // Result: std::string
  Symbol identifier = {Rule({Pattern(MatchIdentifier)}, YieldMatchedInput)};

  // Result: std::string
  Symbol stringLiteral = {
      Rule({Pattern(MatchStringLiteral)}, YieldMatchedInput)};

  // Result: std::string
  Symbol externalString = {Rule({&stringLiteral}, StringLiteralUnquoteAction)};

  // Result: std::string
  Symbol decimalLiteral = {
      Rule({Pattern(MatchDecimalLiteral)}, YieldMatchedInput),
      Rule({Pattern(MatchHexLiteral)}, YieldMatchedInput)};

  // Result: TypeList
  Symbol* typeList = List<TypeExpression*>(&type, Token(","));

  // Result: TypeExpression*
  Symbol simpleType = {
      Rule({Token("("), &type, Token(")")}),
      Rule({CheckIf(Token("constexpr")), &identifier}, MakeBasicTypeExpression),
      Rule({Token("builtin"), Token("("), typeList, Token(")"), Token("=>"),
            &simpleType},
           MakeFunctionTypeExpression)};

  // Result: TypeExpression*
  Symbol type = {Rule({&simpleType}), Rule({&type, Token("|"), &simpleType},
                                           MakeUnionTypeExpression)};

  // Result: GenericParameters
  Symbol genericParameters = {
      Rule({Token("<"),
            List<std::string>(
                Sequence({&identifier, Token(":"), Token("type")}), Token(",")),
            Token(">")})};

  // Result: TypeList
  Symbol genericSpecializationTypeList = {
      Rule({Token("<"), typeList, Token(">")})};

  // Result: base::Optional<TypeList>
  Symbol* optionalGenericParameters = Optional<TypeList>(&genericParameters);

  // Result: ParameterList
  Symbol typeListMaybeVarArgs = {
      Rule({Token("("), List<TypeExpression*>(Sequence({&type, Token(",")})),
            Token("..."), Token(")")},
           MakeParameterListFromTypes<true>),
      Rule({Token("("), typeList, Token(")")},
           MakeParameterListFromTypes<false>)};

  // Result: LabelAndTypes
  Symbol labelParameter = {Rule(
      {&identifier,
       TryOrDefault<TypeList>(Sequence({Token("("), typeList, Token(")")}))},
      MakeLabelAndTypes)};

  // Result: TypeExpression*
  Symbol optionalReturnType = {Rule({Token(":"), &type}),
                               Rule({}, MakeVoidType)};

  // Result: LabelAndTypesVector
  Symbol* optionalLabelList{TryOrDefault<LabelAndTypesVector>(
      Sequence({Token("labels"),
                NonemptyList<LabelAndTypes>(&labelParameter, Token(","))}))};

  // Result: std::vector<Statement*>
  Symbol* optionalOtherwise{TryOrDefault<std::vector<Statement*>>(
      Sequence({Token("otherwise"),
                NonemptyList<Statement*>(&atomarStatement, Token(","))}))};

  // Result: NameAndTypeExpression
  Symbol nameAndType = {
      Rule({&identifier, Token(":"), &type}, MakeNameAndType)};

  // Result: ParameterList
  Symbol parameterListNoVararg = {
      Rule({Token("("), List<NameAndTypeExpression>(&nameAndType, Token(",")),
            Token(")")},
           MakeParameterListFromNameAndTypeList<false>)};

  // Result: ParameterList
  Symbol parameterListAllowVararg = {
      Rule({&parameterListNoVararg}),
      Rule({Token("("),
            NonemptyList<NameAndTypeExpression>(&nameAndType, Token(",")),
            Token(","), Token("..."), &identifier, Token(")")},
           MakeParameterListFromNameAndTypeList<true>)};

  // Result: std::string
  Symbol* OneOf(const std::vector<std::string>& alternatives) {
    Symbol* result = NewSymbol();
    for (const std::string& s : alternatives) {
      result->AddRule(Rule({Token(s)}, YieldMatchedInput));
    }
    return result;
  }

  // Result: Expression*
  Symbol* BinaryOperator(Symbol* nextLevel, Symbol* op) {
    Symbol* result = NewSymbol();
    *result = {Rule({nextLevel}),
               Rule({result, op, nextLevel}, MakeBinaryOperator)};
    return result;
  }

  // Result: Expression*
  Symbol* expression = &assignmentExpression;

  // Result: IncrementDecrementOperator
  Symbol incrementDecrementOperator = {
      Rule({Token("++")},
           YieldIntegralConstant<IncrementDecrementOperator,
                                 IncrementDecrementOperator::kIncrement>),
      Rule({Token("--")},
           YieldIntegralConstant<IncrementDecrementOperator,
                                 IncrementDecrementOperator::kDecrement>)};

  // Result: LocationExpression*
  Symbol locationExpression = {
      Rule(
          {&identifier, TryOrDefault<TypeList>(&genericSpecializationTypeList)},
          MakeIdentifierExpression),
      Rule({&primaryExpression, Token("."), &identifier},
           MakeFieldAccessExpression),
      Rule({&primaryExpression, Token("["), expression, Token("]")},
           MakeElementAccessExpression)};

  // Result: std::vector<Expression*>
  Symbol argumentList = {Rule(
      {Token("("), List<Expression*>(expression, Token(",")), Token(")")})};

  // Result: Expression*
  Symbol callExpression = {
      Rule({&identifier, TryOrDefault<TypeList>(&genericSpecializationTypeList),
            &argumentList, optionalOtherwise},
           MakeCall)};

  // Result: Expression*
  Symbol primaryExpression = {
      Rule({&callExpression}),
      Rule({&locationExpression},
           CastParseResult<LocationExpression*, Expression*>),
      Rule({&decimalLiteral}, MakeNumberLiteralExpression),
      Rule({&stringLiteral}, MakeStringLiteralExpression),
      Rule({&identifier, Token("{"), List<Expression*>(expression, Token(",")),
            Token("}")},
           MakeStructExpression),
      Rule({Token("("), expression, Token(")")})};

  // Result: Expression*
  Symbol unaryExpression = {
      Rule({&primaryExpression}),
      Rule({OneOf({"+", "-", "!", "~"}), &unaryExpression}, MakeUnaryOperator),
      Rule({&incrementDecrementOperator, &locationExpression},
           MakeIncrementDecrementExpressionPrefix),
      Rule({&locationExpression, &incrementDecrementOperator},
           MakeIncrementDecrementExpressionPostfix)};

  // Result: Expression*
  Symbol* multiplicativeExpression =
      BinaryOperator(&unaryExpression, OneOf({"*", "/", "%"}));

  // Result: Expression*
  Symbol* additiveExpression =
      BinaryOperator(multiplicativeExpression, OneOf({"+", "-"}));

  // Result: Expression*
  Symbol* shiftExpression =
      BinaryOperator(additiveExpression, OneOf({"<<", ">>", ">>>"}));

  // Do not allow expressions like a < b > c because this is never
  // useful and ambiguous with template parameters.
  // Result: Expression*
  Symbol relationalExpression = {
      Rule({shiftExpression}),
      Rule({shiftExpression, OneOf({"<", ">", "<=", ">="}), shiftExpression},
           MakeBinaryOperator)};

  // Result: Expression*
  Symbol* equalityExpression =
      BinaryOperator(&relationalExpression, OneOf({"==", "!="}));

  // Result: Expression*
  Symbol* bitwiseExpression =
      BinaryOperator(equalityExpression, OneOf({"&", "|"}));

  // Result: Expression*
  Symbol logicalAndExpression = {
      Rule({bitwiseExpression}),
      Rule({&logicalAndExpression, Token("&&"), bitwiseExpression},
           MakeLogicalAndExpression)};

  // Result: Expression*
  Symbol logicalOrExpression = {
      Rule({&logicalAndExpression}),
      Rule({&logicalOrExpression, Token("||"), &logicalAndExpression},
           MakeLogicalOrExpression)};

  // Result: Expression*
  Symbol conditionalExpression = {
      Rule({&logicalOrExpression}),
      Rule({&logicalOrExpression, Token("?"), expression, Token(":"),
            &conditionalExpression},
           MakeConditionalExpression)};

  // Result: base::Optional<std::string>
  Symbol assignmentOperator = {
      Rule({Token("=")}, YieldDefaultValue<base::Optional<std::string>>),
      Rule({OneOf({"*=", "/=", "%=", "+=", "-=", "<<=", ">>=", ">>>=", "&=",
                   "^=", "|="})},
           ExtractAssignmentOperator)};

  // Result: Expression*
  Symbol assignmentExpression = {
      Rule({&conditionalExpression}),
      Rule({&locationExpression, &assignmentOperator, &assignmentExpression},
           MakeAssignmentExpression)};

  // Result: Statement*
  Symbol block = {Rule({CheckIf(Token("deferred")), Token("{"),
                        List<Statement*>(&statement), Token("}")},
                       MakeBlockStatement)};

  // Result: LabelBlock*
  Symbol labelBlock = {
      Rule({Token("label"), &identifier,
            TryOrDefault<ParameterList>(&parameterListNoVararg), &block},
           MakeLabelBlock)};

  // Result: ExpressionWithSource
  Symbol expressionWithSource = {Rule({expression}, MakeExpressionWithSource)};

  // Result: RangeExpression
  Symbol rangeSpecifier = {
      Rule({Token("["), Optional<Expression*>(expression), Token(":"),
            Optional<Expression*>(expression), Token("]")},
           MakeRangeExpression)};

  // Result: Statement*
  Symbol varDeclaration = {
      Rule({OneOf({"let", "const"}), &identifier, Token(":"), &type},
           MakeVarDeclarationStatement)};

  // Result: Statement*
  Symbol varDeclarationWithInitialization = {
      Rule({OneOf({"let", "const"}), &identifier, Token(":"), &type, Token("="),
            expression},
           MakeVarDeclarationStatement)};

  // Result: Statement*
  Symbol atomarStatement = {
      Rule({expression}, MakeExpressionStatement),
      Rule({Token("return"), Optional<Expression*>(expression)},
           MakeReturnStatement),
      Rule({Token("tail"), &callExpression}, MakeTailCallStatement),
      Rule({Token("break")}, MakeBreakStatement),
      Rule({Token("continue")}, MakeContinueStatement),
      Rule({Token("goto"), &identifier,
            TryOrDefault<std::vector<Expression*>>(&argumentList)},
           MakeGotoStatement),
      Rule({OneOf({"debug", "unreachable"})}, MakeDebugStatement)};

  // Result: Statement*
  Symbol statement = {
      Rule({&block}),
      Rule({&atomarStatement, Token(";")}),
      Rule({&varDeclaration, Token(";")}),
      Rule({&varDeclarationWithInitialization, Token(";")}),
      Rule({Token("if"), CheckIf(Token("constexpr")), Token("("), expression,
            Token(")"), &statement,
            Optional<Statement*>(Sequence({Token("else"), &statement}))},
           MakeIfStatement),
      Rule(
          {
              Token("typeswitch"), Token("("), expression, Token(")"),
              Token("{"), NonemptyList<TypeswitchCase>(&typeswitchCase),
              Token("}"),
          },
          MakeTypeswitchStatement),
      Rule({Token("try"), &block, NonemptyList<LabelBlock*>(&labelBlock)},
           MakeTryLabelExpression),
      Rule({OneOf({"assert", "check"}), Token("("), &expressionWithSource,
            Token(")"), Token(";")},
           MakeAssertStatement),
      Rule({Token("while"), Token("("), expression, Token(")"), &statement},
           MakeWhileStatement),
      Rule({Token("for"), Token("("), &varDeclaration, Token("of"), expression,
            Optional<RangeExpression>(&rangeSpecifier), Token(")"), &statement},
           MakeForOfLoopStatement),
      Rule({Token("for"), Token("("),
            Optional<Statement*>(&varDeclarationWithInitialization), Token(";"),
            Optional<Expression*>(expression), Token(";"),
            Optional<Expression*>(expression), Token(")"), &statement},
           MakeForLoopStatement)};

  // Result: TypeswitchCase
  Symbol typeswitchCase = {
      Rule({Token("case"), Token("("),
            Optional<std::string>(Sequence({&identifier, Token(":")})), &type,
            Token(")"), Token(":"), &block},
           MakeTypeswitchCase)};

  // Result: base::Optional<Statement*>
  Symbol optionalBody = {
      Rule({&block}, CastParseResult<Statement*, base::Optional<Statement*>>),
      Rule({Token(";")}, YieldDefaultValue<base::Optional<Statement*>>)};

  // Result: Declaration*
  Symbol declaration = {
      Rule({Token("const"), &identifier, Token(":"), &type, Token("="),
            expression, Token(";")},
           MakeConstDeclaration),
      Rule({Token("const"), &identifier, Token(":"), &type, Token("generates"),
            &externalString, Token(";")},
           MakeExternConstDeclaration),
      Rule({Token("type"), &identifier,
            Optional<std::string>(Sequence({Token("extends"), &identifier})),
            Optional<std::string>(
                Sequence({Token("generates"), &externalString})),
            Optional<std::string>(
                Sequence({Token("constexpr"), &externalString})),
            Token(";")},
           MakeTypeDeclaration),
      Rule({Token("type"), &identifier, Token("="), &type, Token(";")},
           MakeTypeAliasDeclaration),
      Rule({Token("extern"),
            Optional<std::string>(
                Sequence({Token("operator"), &externalString})),
            Token("macro"), &identifier,
            TryOrDefault<GenericParameters>(&genericParameters),
            &typeListMaybeVarArgs, &optionalReturnType, optionalLabelList,
            Token(";")},
           MakeExternalMacro),
      Rule({Token("extern"), CheckIf(Token("javascript")), Token("builtin"),
            &identifier, TryOrDefault<GenericParameters>(&genericParameters),
            &typeListMaybeVarArgs, &optionalReturnType, Token(";")},
           MakeExternalBuiltin),
      Rule({Token("extern"), Token("runtime"), &identifier,
            &typeListMaybeVarArgs, &optionalReturnType, Token(";")},
           MakeExternalRuntime),
      Rule({Optional<std::string>(
                Sequence({Token("operator"), &externalString})),
            Token("macro"), &identifier,
            TryOrDefault<GenericParameters>(&genericParameters),
            &parameterListNoVararg, &optionalReturnType, optionalLabelList,
            &optionalBody},
           MakeTorqueMacroDeclaration),
      Rule({CheckIf(Token("javascript")), Token("builtin"), &identifier,
            TryOrDefault<GenericParameters>(&genericParameters),
            &parameterListAllowVararg, &optionalReturnType, &optionalBody},
           MakeTorqueBuiltinDeclaration),
      Rule({&identifier, &genericSpecializationTypeList,
            &parameterListAllowVararg, &optionalReturnType, optionalLabelList,
            &block},
           MakeSpecializationDeclaration),
      Rule({Token("struct"), &identifier, Token("{"),
            List<NameAndTypeExpression>(Sequence({&nameAndType, Token(";")})),
            Token("}")},
           MakeStructDeclaration)};

  // Result: Declaration*
  Symbol moduleDeclaration = {
      Rule({Token("module"), &identifier, Token("{"),
            List<Declaration*>(&declaration), Token("}")},
           MakeExplicitModuleDeclaration)};

  Symbol file = {Rule({&file, &moduleDeclaration}, AddGlobalDeclaration),
                 Rule({&file, &declaration}, AddGlobalDeclaration), Rule({})};
};

}  // namespace

void ParseTorque(const std::string& input) { TorqueGrammar().Parse(input); }

}  // namespace torque
}  // namespace internal
}  // namespace v8
