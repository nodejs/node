// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>

#include "src/common/globals.h"
#include "src/torque/constants.h"
#include "src/torque/declarations.h"
#include "src/torque/earley-parser.h"
#include "src/torque/torque-parser.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(CurrentAst)

using TypeList = std::vector<TypeExpression*>;
using GenericParameters = std::vector<Identifier*>;

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

class BuildFlags : public ContextualClass<BuildFlags> {
 public:
  BuildFlags() {
    build_flags_["V8_SFI_HAS_UNIQUE_ID"] = V8_SFI_HAS_UNIQUE_ID;
    build_flags_["TAGGED_SIZE_8_BYTES"] = TAGGED_SIZE_8_BYTES;
    build_flags_["V8_DOUBLE_FIELDS_UNBOXING"] = V8_DOUBLE_FIELDS_UNBOXING;
    build_flags_["TRUE_FOR_TESTING"] = true;
    build_flags_["FALSE_FOR_TESTING"] = false;
  }
  static bool GetFlag(const std::string& name, const char* production) {
    auto it = Get().build_flags_.find(name);
    if (it == Get().build_flags_.end()) {
      ReportError("Unknown flag used in ", production, ": ", name,
                  ". Please add it to the list in BuildFlags.");
    }
    return it->second;
  }

 private:
  std::unordered_map<std::string, bool> build_flags_;
};
DEFINE_CONTEXTUAL_VARIABLE(BuildFlags)

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
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<TypeExpression*>>::id =
        ParseResultTypeId::kOptionalTypeExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<LabelBlock*>::id =
    ParseResultTypeId::kLabelBlockPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<LabelBlock*>>::id =
        ParseResultTypeId::kOptionalLabelBlockPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Expression*>::id =
    ParseResultTypeId::kExpressionPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Identifier*>::id =
    ParseResultTypeId::kIdentifierPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<Identifier*>>::id =
        ParseResultTypeId::kOptionalIdentifierPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Statement*>::id =
    ParseResultTypeId::kStatementPtr;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<NameAndTypeExpression>::id =
        ParseResultTypeId::kNameAndTypeExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<NameAndExpression>::id =
        ParseResultTypeId::kNameAndExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId ParseResultHolder<Annotation>::id =
    ParseResultTypeId::kAnnotation;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<Annotation>>::id =
        ParseResultTypeId::kVectorOfAnnotation;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<ClassFieldExpression>::id =
        ParseResultTypeId::kClassFieldExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<StructFieldExpression>::id =
        ParseResultTypeId::kStructFieldExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<NameAndTypeExpression>>::id =
        ParseResultTypeId::kStdVectorOfNameAndTypeExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<ImplicitParameters>::id =
        ParseResultTypeId::kImplicitParameters;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<base::Optional<ImplicitParameters>>::id =
        ParseResultTypeId::kOptionalImplicitParameters;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<NameAndExpression>>::id =
        ParseResultTypeId::kStdVectorOfNameAndExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<ClassFieldExpression>>::id =
        ParseResultTypeId::kStdVectorOfClassFieldExpression;
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<StructFieldExpression>>::id =
        ParseResultTypeId::kStdVectorOfStructFieldExpression;
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
    ParseResultHolder<std::vector<std::vector<Declaration*>>>::id =
        ParseResultTypeId::kStdVectorOfStdVectorOfDeclarationPtr;
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
template <>
V8_EXPORT_PRIVATE const ParseResultTypeId
    ParseResultHolder<std::vector<Identifier*>>::id =
        ParseResultTypeId::kStdVectorOfIdentifierPtr;

namespace {

base::Optional<ParseResult> AddGlobalDeclarations(
    ParseResultIterator* child_results) {
  auto declarations = child_results->NextAs<std::vector<Declaration*>>();
  for (Declaration* declaration : declarations) {
    CurrentAst::Get().declarations().push_back(declaration);
  }
  return base::nullopt;
}

void NamingConventionError(const std::string& type, const std::string& name,
                           const std::string& convention,
                           SourcePosition pos = CurrentSourcePosition::Get()) {
  Lint(type, " \"", name, "\" does not follow \"", convention,
       "\" naming convention.")
      .Position(pos);
}

void NamingConventionError(const std::string& type, const Identifier* name,
                           const std::string& convention) {
  NamingConventionError(type, name->value, convention, name->pos);
}

void LintGenericParameters(const GenericParameters& parameters) {
  for (auto parameter : parameters) {
    if (!IsUpperCamelCase(parameter->value)) {
      NamingConventionError("Generic parameter", parameter, "UpperCamelCase");
    }
  }
}

base::Optional<ParseResult> ConcatList(ParseResultIterator* child_results) {
  auto list_of_lists =
      child_results->NextAs<std::vector<std::vector<Declaration*>>>();
  std::vector<Declaration*> result;
  for (auto& list : list_of_lists) {
    result.insert(result.end(), list.begin(), list.end());
  }
  return ParseResult{result};
}

void CheckNotDeferredStatement(Statement* statement) {
  CurrentSourcePosition::Scope source_position(statement->pos);
  if (BlockStatement* block = BlockStatement::DynamicCast(statement)) {
    if (block->deferred) {
      Lint(
          "cannot use deferred with a statement block here, it will have no "
          "effect");
    }
  }
}

Expression* MakeCall(IdentifierExpression* callee,
                     base::Optional<Expression*> target,
                     std::vector<Expression*> arguments,
                     const std::vector<Statement*>& otherwise) {
  std::vector<Identifier*> labels;

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
    auto label_name = std::string("__label") + std::to_string(label_id++);
    auto label_id = MakeNode<Identifier>(label_name);
    label_id->pos = SourcePosition::Invalid();
    labels.push_back(label_id);
    auto* label_block =
        MakeNode<LabelBlock>(label_id, ParameterList::Empty(), statement);
    temp_labels.push_back(label_block);
  }

  // Create nested try-label expression for all of the temporary Labels that
  // were created.
  Expression* result = nullptr;
  if (target) {
    result = MakeNode<CallMethodExpression>(*target, callee, arguments, labels);
  } else {
    result = MakeNode<CallExpression>(callee, arguments, labels);
  }

  for (auto* label : temp_labels) {
    result = MakeNode<TryLabelExpression>(false, result, label);
  }
  return result;
}

Expression* MakeCall(Identifier* callee,
                     const std::vector<TypeExpression*>& generic_arguments,
                     const std::vector<Expression*>& arguments,
                     const std::vector<Statement*>& otherwise) {
  return MakeCall(MakeNode<IdentifierExpression>(callee, generic_arguments),
                  base::nullopt, arguments, otherwise);
}

base::Optional<ParseResult> MakeCall(ParseResultIterator* child_results) {
  auto callee = child_results->NextAs<Expression*>();
  auto args = child_results->NextAs<std::vector<Expression*>>();
  auto otherwise = child_results->NextAs<std::vector<Statement*>>();
  IdentifierExpression* target = IdentifierExpression::cast(callee);
  return ParseResult{MakeCall(target, base::nullopt, args, otherwise)};
}

base::Optional<ParseResult> MakeMethodCall(ParseResultIterator* child_results) {
  auto this_arg = child_results->NextAs<Expression*>();
  auto callee = child_results->NextAs<std::string>();
  auto args = child_results->NextAs<std::vector<Expression*>>();
  auto otherwise = child_results->NextAs<std::vector<Statement*>>();
  return ParseResult{
      MakeCall(MakeNode<IdentifierExpression>(MakeNode<Identifier>(callee)),
               this_arg, args, otherwise)};
}

base::Optional<ParseResult> MakeNewExpression(
    ParseResultIterator* child_results) {
  auto type = child_results->NextAs<TypeExpression*>();
  auto initializers = child_results->NextAs<std::vector<NameAndExpression>>();
  Expression* result = MakeNode<NewExpression>(type, std::move(initializers));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeBinaryOperator(
    ParseResultIterator* child_results) {
  auto left = child_results->NextAs<Expression*>();
  auto op = child_results->NextAs<Identifier*>();
  auto right = child_results->NextAs<Expression*>();
  return ParseResult{MakeCall(op, TypeList{},
                              std::vector<Expression*>{left, right},
                              std::vector<Statement*>{})};
}

base::Optional<ParseResult> MakeIntrinsicCallExpression(
    ParseResultIterator* child_results) {
  auto callee = child_results->NextAs<Identifier*>();
  auto generic_arguments =
      child_results->NextAs<std::vector<TypeExpression*>>();
  auto args = child_results->NextAs<std::vector<Expression*>>();
  Expression* result =
      MakeNode<IntrinsicCallExpression>(callee, generic_arguments, args);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeUnaryOperator(
    ParseResultIterator* child_results) {
  auto op = child_results->NextAs<Identifier*>();
  auto e = child_results->NextAs<Expression*>();
  return ParseResult{MakeCall(op, TypeList{}, std::vector<Expression*>{e},
                              std::vector<Statement*>{})};
}

base::Optional<ParseResult> MakeSpreadExpression(
    ParseResultIterator* child_results) {
  auto spreadee = child_results->NextAs<Expression*>();
  Expression* result = MakeNode<SpreadExpression>(spreadee);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeImplicitParameterList(
    ParseResultIterator* child_results) {
  auto kind = child_results->NextAs<Identifier*>();
  auto parameters = child_results->NextAs<std::vector<NameAndTypeExpression>>();
  return ParseResult{ImplicitParameters{kind, parameters}};
}

void AddParameter(ParameterList* parameter_list,
                  const NameAndTypeExpression& param) {
  if (!IsLowerCamelCase(param.name->value)) {
    NamingConventionError("Parameter", param.name, "lowerCamelCase");
  }
  parameter_list->names.push_back(param.name);
  parameter_list->types.push_back(param.type);
}

template <bool has_varargs, bool has_explicit_parameter_names>
base::Optional<ParseResult> MakeParameterList(
    ParseResultIterator* child_results) {
  auto implicit_params =
      child_results->NextAs<base::Optional<ImplicitParameters>>();
  ParameterList result;
  result.has_varargs = has_varargs;
  result.implicit_count = 0;
  result.implicit_kind = ImplicitKind::kNoImplicit;
  if (implicit_params) {
    result.implicit_count = implicit_params->parameters.size();
    if (implicit_params->kind->value == "implicit") {
      result.implicit_kind = ImplicitKind::kImplicit;
    } else {
      DCHECK_EQ(implicit_params->kind->value, "js-implicit");
      result.implicit_kind = ImplicitKind::kJSImplicit;
    }
    result.implicit_kind_pos = implicit_params->kind->pos;
    for (NameAndTypeExpression& implicit_param : implicit_params->parameters) {
      AddParameter(&result, implicit_param);
    }
  }
  if (has_explicit_parameter_names) {
    auto explicit_params =
        child_results->NextAs<std::vector<NameAndTypeExpression>>();
    std::string arguments_variable = "";
    if (has_varargs) {
      arguments_variable = child_results->NextAs<std::string>();
    }
    for (NameAndTypeExpression& param : explicit_params) {
      AddParameter(&result, param);
    }
    result.arguments_variable = arguments_variable;
  } else {
    auto explicit_types = child_results->NextAs<TypeList>();
    for (auto* explicit_type : explicit_types) {
      result.types.push_back(explicit_type);
    }
  }
  return ParseResult{std::move(result)};
}

base::Optional<ParseResult> MakeAssertStatement(
    ParseResultIterator* child_results) {
  auto kind = child_results->NextAs<Identifier*>()->value;
  auto expr_with_source = child_results->NextAs<ExpressionWithSource>();
  DCHECK(kind == "assert" || kind == "check");
  Statement* result = MakeNode<AssertStatement>(
      kind == "assert", expr_with_source.expression, expr_with_source.source);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeDebugStatement(
    ParseResultIterator* child_results) {
  auto kind = child_results->NextAs<Identifier*>()->value;
  DCHECK(kind == "unreachable" || kind == "debug");
  Statement* result = MakeNode<DebugStatement>(kind, kind == "unreachable");
  return ParseResult{result};
}

base::Optional<ParseResult> MakeVoidType(ParseResultIterator* child_results) {
  TypeExpression* result = MakeNode<BasicTypeExpression>(
      std::vector<std::string>{}, "void", std::vector<TypeExpression*>{});
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternalMacro(
    ParseResultIterator* child_results) {
  auto transitioning = child_results->NextAs<bool>();
  auto operator_name = child_results->NextAs<base::Optional<std::string>>();
  auto external_assembler_name =
      child_results->NextAs<base::Optional<std::string>>();
  auto name = child_results->NextAs<Identifier*>();
  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();

  Declaration* result = MakeNode<ExternalMacroDeclaration>(
      transitioning,
      external_assembler_name ? *external_assembler_name : "CodeStubAssembler",
      name, operator_name, args, return_type, labels);
  if (!generic_parameters.empty()) {
    Error("External builtins cannot be generic.");
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeIntrinsicDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto body = child_results->NextAs<base::Optional<Statement*>>();
  LabelAndTypesVector labels;
  CallableDeclaration* declaration;
  if (body) {
    declaration = MakeNode<TorqueMacroDeclaration>(
        false, name, base::Optional<std::string>{}, args, return_type, labels,
        false, body);
  } else {
    declaration = MakeNode<IntrinsicDeclaration>(name, args, return_type);
  }
  Declaration* result = declaration;
  if (!generic_parameters.empty()) {
    result = MakeNode<GenericDeclaration>(generic_parameters, declaration);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTorqueMacroDeclaration(
    ParseResultIterator* child_results) {
  auto export_to_csa = child_results->NextAs<bool>();
  auto transitioning = child_results->NextAs<bool>();
  auto operator_name = child_results->NextAs<base::Optional<std::string>>();
  auto name = child_results->NextAs<Identifier*>();
  if (!IsUpperCamelCase(name->value)) {
    NamingConventionError("Macro", name, "UpperCamelCase");
  }

  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();
  auto body = child_results->NextAs<base::Optional<Statement*>>();
  CallableDeclaration* declaration = MakeNode<TorqueMacroDeclaration>(
      transitioning, name, operator_name, args, return_type, labels,
      export_to_csa, body);
  Declaration* result = declaration;
  if (generic_parameters.empty()) {
    if (!body) ReportError("A non-generic declaration needs a body.");
  } else {
    if (export_to_csa) ReportError("Cannot export generics to CSA.");
    result = MakeNode<GenericDeclaration>(generic_parameters, declaration);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTorqueBuiltinDeclaration(
    ParseResultIterator* child_results) {
  auto transitioning = child_results->NextAs<bool>();
  auto javascript_linkage = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  if (!IsUpperCamelCase(name->value)) {
    NamingConventionError("Builtin", name, "UpperCamelCase");
  }

  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto body = child_results->NextAs<base::Optional<Statement*>>();
  CallableDeclaration* declaration = MakeNode<TorqueBuiltinDeclaration>(
      transitioning, javascript_linkage, name, args, return_type, body);
  Declaration* result = declaration;
  if (generic_parameters.empty()) {
    if (!body) ReportError("A non-generic declaration needs a body.");
  } else {
    result = MakeNode<GenericDeclaration>(generic_parameters, declaration);
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeConstDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  if (!IsValidNamespaceConstName(name->value)) {
    NamingConventionError("Constant", name, "kUpperCamelCase");
  }

  auto type = child_results->NextAs<TypeExpression*>();
  auto expression = child_results->NextAs<Expression*>();
  Declaration* result = MakeNode<ConstDeclaration>(name, type, expression);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternConstDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  auto type = child_results->NextAs<TypeExpression*>();
  auto literal = child_results->NextAs<std::string>();
  Declaration* result =
      MakeNode<ExternConstDeclaration>(name, type, std::move(literal));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTypeAliasDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  auto type = child_results->NextAs<TypeExpression*>();
  Declaration* result = MakeNode<TypeAliasDeclaration>(name, type);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeAbstractTypeDeclaration(
    ParseResultIterator* child_results) {
  auto transient = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  if (!IsValidTypeName(name->value)) {
    NamingConventionError("Type", name, "UpperCamelCase");
  }
  auto extends = child_results->NextAs<base::Optional<Identifier*>>();
  auto generates = child_results->NextAs<base::Optional<std::string>>();
  Declaration* decl = MakeNode<AbstractTypeDeclaration>(
      name, transient, extends, std::move(generates));

  auto constexpr_generates =
      child_results->NextAs<base::Optional<std::string>>();
  std::vector<Declaration*> result{decl};

  if (constexpr_generates) {
    // Create a AbstractTypeDeclaration for the associated constexpr type.
    Identifier* constexpr_name =
        MakeNode<Identifier>(CONSTEXPR_TYPE_PREFIX + name->value);
    constexpr_name->pos = name->pos;

    base::Optional<Identifier*> constexpr_extends;
    if (extends) {
      constexpr_extends =
          MakeNode<Identifier>(CONSTEXPR_TYPE_PREFIX + (*extends)->value);
      (*constexpr_extends)->pos = name->pos;
    }
    AbstractTypeDeclaration* constexpr_decl = MakeNode<AbstractTypeDeclaration>(
        constexpr_name, transient, constexpr_extends, constexpr_generates);
    constexpr_decl->pos = name->pos;
    result.push_back(constexpr_decl);
  }

  return ParseResult{result};
}

base::Optional<ParseResult> MakeMethodDeclaration(
    ParseResultIterator* child_results) {
  auto transitioning = child_results->NextAs<bool>();
  auto operator_name = child_results->NextAs<base::Optional<std::string>>();
  auto name = child_results->NextAs<Identifier*>();
  if (!IsUpperCamelCase(name->value)) {
    NamingConventionError("Method", name, "UpperCamelCase");
  }

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();
  auto body = child_results->NextAs<Statement*>();
  Declaration* result =
      MakeNode<TorqueMacroDeclaration>(transitioning, name, operator_name, args,
                                       return_type, labels, false, body);
  return ParseResult{result};
}

class AnnotationSet {
 public:
  AnnotationSet(ParseResultIterator* iter,
                const std::set<std::string>& allowed_without_param,
                const std::set<std::string>& allowed_with_param) {
    auto list = iter->NextAs<std::vector<Annotation>>();
    for (const Annotation& a : list) {
      if (a.param.has_value()) {
        if (allowed_with_param.find(a.name->value) ==
            allowed_with_param.end()) {
          const char* error_message =
              allowed_without_param.find(a.name->value) ==
                      allowed_without_param.end()
                  ? " is not allowed here"
                  : " cannot have parameter here";
          Lint("Annotation ", a.name->value, error_message)
              .Position(a.name->pos);
        }
        map_[a.name->value].push_back(*a.param);
      } else {
        if (allowed_without_param.find(a.name->value) ==
            allowed_without_param.end()) {
          const char* error_message =
              allowed_with_param.find(a.name->value) == allowed_with_param.end()
                  ? " is not allowed here"
                  : " requires a parameter here";
          Lint("Annotation ", a.name->value, error_message)
              .Position(a.name->pos);
        }
        if (!set_.insert(a.name->value).second) {
          Lint("Duplicate annotation ", a.name->value).Position(a.name->pos);
        }
      }
    }
  }

  bool Contains(const std::string& s) { return set_.find(s) != set_.end(); }
  const std::vector<std::string>& GetParams(const std::string& s) {
    return map_[s];
  }

 private:
  std::set<std::string> set_;
  std::map<std::string, std::vector<std::string>> map_;
};

base::Optional<ParseResult> MakeClassDeclaration(
    ParseResultIterator* child_results) {
  AnnotationSet annotations(
      child_results,
      {"@generatePrint", "@noVerifier", "@abstract",
       "@dirtyInstantiatedAbstractClass", "@hasSameInstanceTypeAsParent",
       "@generateCppClass"},
      {});
  ClassFlags flags = ClassFlag::kNone;
  bool generate_print = annotations.Contains("@generatePrint");
  if (generate_print) flags |= ClassFlag::kGeneratePrint;
  bool generate_verify = !annotations.Contains("@noVerifier");
  if (generate_verify) flags |= ClassFlag::kGenerateVerify;
  if (annotations.Contains("@abstract")) {
    flags |= ClassFlag::kAbstract;
  }
  if (annotations.Contains("@dirtyInstantiatedAbstractClass")) {
    flags |= ClassFlag::kInstantiatedAbstractClass;
  }
  if (annotations.Contains("@hasSameInstanceTypeAsParent")) {
    flags |= ClassFlag::kHasSameInstanceTypeAsParent;
  }
  if (annotations.Contains("@generateCppClass")) {
    flags |= ClassFlag::kGenerateCppClassDefinitions;
  }
  auto is_extern = child_results->NextAs<bool>();
  if (is_extern) flags |= ClassFlag::kExtern;
  auto transient = child_results->NextAs<bool>();
  if (transient) flags |= ClassFlag::kTransient;
  auto name = child_results->NextAs<Identifier*>();
  if (!IsValidTypeName(name->value)) {
    NamingConventionError("Type", name, "UpperCamelCase");
  }
  auto extends = child_results->NextAs<base::Optional<TypeExpression*>>();
  if (extends && !BasicTypeExpression::DynamicCast(*extends)) {
    ReportError("Expected type name in extends clause.");
  }
  auto generates = child_results->NextAs<base::Optional<std::string>>();
  auto methods = child_results->NextAs<std::vector<Declaration*>>();
  auto fields_raw = child_results->NextAs<std::vector<ClassFieldExpression>>();

  // Filter to only include fields that should be present based on decoration.
  std::vector<ClassFieldExpression> fields;
  std::copy_if(
      fields_raw.begin(), fields_raw.end(), std::back_inserter(fields),
      [](const ClassFieldExpression& exp) {
        for (const ConditionalAnnotation& condition : exp.conditions) {
          if (condition.type == ConditionalAnnotationType::kPositive
                  ? !BuildFlags::GetFlag(condition.condition, "@if")
                  : BuildFlags::GetFlag(condition.condition, "@ifnot")) {
            return false;
          }
        }
        return true;
      });

  Declaration* result = MakeNode<ClassDeclaration>(
      name, flags, std::move(extends), std::move(generates), std::move(methods),
      fields);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeNamespaceDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  if (!IsSnakeCase(name)) {
    NamingConventionError("Namespace", name, "snake_case");
  }
  auto declarations = child_results->NextAs<std::vector<Declaration*>>();
  Declaration* result =
      MakeNode<NamespaceDeclaration>(std::move(name), std::move(declarations));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeSpecializationDeclaration(
    ParseResultIterator* child_results) {
  auto transitioning = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  auto generic_parameters =
      child_results->NextAs<std::vector<TypeExpression*>>();
  auto parameters = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  auto labels = child_results->NextAs<LabelAndTypesVector>();
  auto body = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(body);
  Declaration* result = MakeNode<SpecializationDeclaration>(
      transitioning, std::move(name), std::move(generic_parameters),
      std::move(parameters), return_type, std::move(labels), body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeStructDeclaration(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  if (!IsValidTypeName(name->value)) {
    NamingConventionError("Struct", name, "UpperCamelCase");
  }
  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);
  auto methods = child_results->NextAs<std::vector<Declaration*>>();
  auto fields = child_results->NextAs<std::vector<StructFieldExpression>>();
  Declaration* result =
      MakeNode<StructDeclaration>(name, std::move(methods), std::move(fields),
                                  std::move(generic_parameters));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeCppIncludeDeclaration(
    ParseResultIterator* child_results) {
  auto include_path = child_results->NextAs<std::string>();
  Declaration* result =
      MakeNode<CppIncludeDeclaration>(std::move(include_path));
  return ParseResult{result};
}

base::Optional<ParseResult> ProcessTorqueImportDeclaration(
    ParseResultIterator* child_results) {
  auto import_path = child_results->NextAs<std::string>();
  if (!SourceFileMap::FileRelativeToV8RootExists(import_path)) {
    Error("File '", import_path, "' not found.");
  }

  auto import_id = SourceFileMap::GetSourceId(import_path);
  if (!import_id.IsValid()) {
    // TODO(szuend): Instead of reporting and error. Queue the file up
    //               for compilation.
    Error("File '", import_path, "'is not part of the source set.").Throw();
  }

  CurrentAst::Get().DeclareImportForCurrentFile(import_id);

  return base::nullopt;
}

base::Optional<ParseResult> MakeExternalBuiltin(
    ParseResultIterator* child_results) {
  auto transitioning = child_results->NextAs<bool>();
  auto js_linkage = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  auto generic_parameters = child_results->NextAs<GenericParameters>();
  LintGenericParameters(generic_parameters);

  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  Declaration* result = MakeNode<ExternalBuiltinDeclaration>(
      transitioning, js_linkage, name, args, return_type);
  if (!generic_parameters.empty()) {
    Error("External builtins cannot be generic.");
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExternalRuntime(
    ParseResultIterator* child_results) {
  auto transitioning = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  auto args = child_results->NextAs<ParameterList>();
  auto return_type = child_results->NextAs<TypeExpression*>();
  Declaration* result = MakeNode<ExternalRuntimeDeclaration>(
      transitioning, name, args, return_type);
  return ParseResult{result};
}

base::Optional<ParseResult> StringLiteralUnquoteAction(
    ParseResultIterator* child_results) {
  return ParseResult{
      StringLiteralUnquote(child_results->NextAs<std::string>())};
}

base::Optional<ParseResult> MakeBasicTypeExpression(
    ParseResultIterator* child_results) {
  auto namespace_qualification =
      child_results->NextAs<std::vector<std::string>>();
  auto is_constexpr = child_results->NextAs<bool>();
  auto name = child_results->NextAs<std::string>();
  auto generic_arguments =
      child_results->NextAs<std::vector<TypeExpression*>>();
  TypeExpression* result = MakeNode<BasicTypeExpression>(
      std::move(namespace_qualification),
      is_constexpr ? GetConstexprName(name) : std::move(name),
      std::move(generic_arguments));
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

base::Optional<ParseResult> MakeReferenceTypeExpression(
    ParseResultIterator* child_results) {
  auto referenced_type = child_results->NextAs<TypeExpression*>();
  std::vector<std::string> namespace_qualification{
      TORQUE_INTERNAL_NAMESPACE_STRING};
  std::vector<TypeExpression*> generic_arguments{referenced_type};
  TypeExpression* result = MakeNode<BasicTypeExpression>(
      namespace_qualification, REFERENCE_TYPE_STRING, generic_arguments);
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

  if (is_constexpr) {
    CheckNotDeferredStatement(if_true);
    if (if_false) CheckNotDeferredStatement(*if_false);
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
        true, MakeNode<Identifier>("__value"), base::nullopt, expression));
  }

  TypeExpression* accumulated_types;
  for (size_t i = 0; i < cases.size(); ++i) {
    CurrentSourcePosition::Scope current_source_position(cases[i].pos);
    Expression* value =
        MakeNode<IdentifierExpression>(MakeNode<Identifier>("__value"));
    if (i >= 1) {
      value =
          MakeNode<AssumeTypeImpossibleExpression>(accumulated_types, value);
    }
    BlockStatement* case_block;
    if (i < cases.size() - 1) {
      value = MakeCall(MakeNode<Identifier>("Cast"),
                       std::vector<TypeExpression*>{cases[i].type},
                       std::vector<Expression*>{value},
                       std::vector<Statement*>{MakeNode<ExpressionStatement>(
                           MakeNode<IdentifierExpression>(
                               MakeNode<Identifier>(kNextCaseLabelName)))});
      case_block = MakeNode<BlockStatement>();
    } else {
      case_block = current_block;
    }
    std::string name = "__case_value";
    if (cases[i].name) name = *cases[i].name;
    case_block->statements.push_back(MakeNode<VarDeclarationStatement>(
        true, MakeNode<Identifier>(name), cases[i].type, value));
    case_block->statements.push_back(cases[i].block);
    if (i < cases.size() - 1) {
      BlockStatement* next_block = MakeNode<BlockStatement>();
      current_block->statements.push_back(
          MakeNode<ExpressionStatement>(MakeNode<TryLabelExpression>(
              false, MakeNode<StatementExpression>(case_block),
              MakeNode<LabelBlock>(MakeNode<Identifier>(kNextCaseLabelName),
                                   ParameterList::Empty(), next_block))));
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
  auto kind = child_results->NextAs<Identifier*>();
  bool const_qualified = kind->value == "const";
  if (!const_qualified) DCHECK_EQ("let", kind->value);
  auto name = child_results->NextAs<Identifier*>();
  if (!IsLowerCamelCase(name->value)) {
    NamingConventionError("Variable", name, "lowerCamelCase");
  }

  auto type = child_results->NextAs<base::Optional<TypeExpression*>>();
  base::Optional<Expression*> initializer;
  if (child_results->HasNext())
    initializer = child_results->NextAs<Expression*>();
  if (!initializer && !type) {
    ReportError("Declaration is missing a type.");
  }
  Statement* result = MakeNode<VarDeclarationStatement>(const_qualified, name,
                                                        type, initializer);
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
  auto label = child_results->NextAs<Identifier*>();
  auto arguments = child_results->NextAs<std::vector<Expression*>>();
  Statement* result = MakeNode<GotoStatement>(label, std::move(arguments));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeBlockStatement(
    ParseResultIterator* child_results) {
  auto deferred = child_results->NextAs<bool>();
  auto statements = child_results->NextAs<std::vector<Statement*>>();
  for (Statement* statement : statements) {
    CheckNotDeferredStatement(statement);
  }
  Statement* result = MakeNode<BlockStatement>(deferred, std::move(statements));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeTryLabelExpression(
    ParseResultIterator* child_results) {
  auto try_block = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(try_block);
  Statement* result = try_block;
  auto label_blocks = child_results->NextAs<std::vector<LabelBlock*>>();
  auto catch_block = child_results->NextAs<base::Optional<LabelBlock*>>();
  for (auto block : label_blocks) {
    result = MakeNode<ExpressionStatement>(MakeNode<TryLabelExpression>(
        false, MakeNode<StatementExpression>(result), block));
  }
  if (catch_block) {
    result = MakeNode<ExpressionStatement>(MakeNode<TryLabelExpression>(
        true, MakeNode<StatementExpression>(result), *catch_block));
  }
  return ParseResult{result};
}

base::Optional<ParseResult> MakeForLoopStatement(
    ParseResultIterator* child_results) {
  auto var_decl = child_results->NextAs<base::Optional<Statement*>>();
  auto test = child_results->NextAs<base::Optional<Expression*>>();
  auto action = child_results->NextAs<base::Optional<Expression*>>();
  base::Optional<Statement*> action_stmt;
  if (action) action_stmt = MakeNode<ExpressionStatement>(*action);
  auto body = child_results->NextAs<Statement*>();
  CheckNotDeferredStatement(body);
  Statement* result =
      MakeNode<ForLoopStatement>(var_decl, test, action_stmt, body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeLabelBlock(ParseResultIterator* child_results) {
  auto label = child_results->NextAs<Identifier*>();
  if (!IsUpperCamelCase(label->value)) {
    NamingConventionError("Label", label, "UpperCamelCase");
  }
  auto parameters = child_results->NextAs<ParameterList>();
  auto body = child_results->NextAs<Statement*>();
  LabelBlock* result = MakeNode<LabelBlock>(label, std::move(parameters), body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeCatchBlock(ParseResultIterator* child_results) {
  auto variable = child_results->NextAs<std::string>();
  auto body = child_results->NextAs<Statement*>();
  if (!IsLowerCamelCase(variable)) {
    NamingConventionError("Exception", variable, "lowerCamelCase");
  }
  ParameterList parameters;
  parameters.names.push_back(MakeNode<Identifier>(variable));
  parameters.types.push_back(MakeNode<BasicTypeExpression>(
      std::vector<std::string>{}, "JSAny", std::vector<TypeExpression*>{}));
  parameters.has_varargs = false;
  LabelBlock* result = MakeNode<LabelBlock>(
      MakeNode<Identifier>(kCatchLabelName), std::move(parameters), body);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeExpressionWithSource(
    ParseResultIterator* child_results) {
  auto e = child_results->NextAs<Expression*>();
  return ParseResult{
      ExpressionWithSource{e, child_results->matched_input().ToString()}};
}

base::Optional<ParseResult> MakeIdentifier(ParseResultIterator* child_results) {
  auto name = child_results->NextAs<std::string>();
  Identifier* result = MakeNode<Identifier>(std::move(name));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeIdentifierFromMatchedInput(
    ParseResultIterator* child_results) {
  return ParseResult{
      MakeNode<Identifier>(child_results->matched_input().ToString())};
}

base::Optional<ParseResult> MakeRightShiftIdentifier(
    ParseResultIterator* child_results) {
  std::string str = child_results->matched_input().ToString();
  for (auto character : str) {
    if (character != '>') {
      ReportError("right-shift operators may not contain any whitespace");
    }
  }
  return ParseResult{MakeNode<Identifier>(str)};
}

base::Optional<ParseResult> MakeIdentifierExpression(
    ParseResultIterator* child_results) {
  auto namespace_qualification =
      child_results->NextAs<std::vector<std::string>>();
  auto name = child_results->NextAs<Identifier*>();
  auto generic_arguments =
      child_results->NextAs<std::vector<TypeExpression*>>();
  Expression* result = MakeNode<IdentifierExpression>(
      std::move(namespace_qualification), name, std::move(generic_arguments));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeFieldAccessExpression(
    ParseResultIterator* child_results) {
  auto object = child_results->NextAs<Expression*>();
  auto field = child_results->NextAs<Identifier*>();
  Expression* result = MakeNode<FieldAccessExpression>(object, field);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeElementAccessExpression(
    ParseResultIterator* child_results) {
  auto object = child_results->NextAs<Expression*>();
  auto field = child_results->NextAs<Expression*>();
  Expression* result = MakeNode<ElementAccessExpression>(object, field);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeDereferenceExpression(
    ParseResultIterator* child_results) {
  auto reference = child_results->NextAs<Expression*>();
  Expression* result = MakeNode<DereferenceExpression>(reference);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeStructExpression(
    ParseResultIterator* child_results) {
  auto type = child_results->NextAs<TypeExpression*>();
  auto initializers = child_results->NextAs<std::vector<NameAndExpression>>();
  Expression* result =
      MakeNode<StructExpression>(type, std::move(initializers));
  return ParseResult{result};
}

base::Optional<ParseResult> MakeAssignmentExpression(
    ParseResultIterator* child_results) {
  auto location = child_results->NextAs<Expression*>();
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
  auto location = child_results->NextAs<Expression*>();
  auto op = child_results->NextAs<IncrementDecrementOperator>();
  Expression* result =
      MakeNode<IncrementDecrementExpression>(location, op, true);
  return ParseResult{result};
}

base::Optional<ParseResult> MakeIncrementDecrementExpressionPrefix(
    ParseResultIterator* child_results) {
  auto op = child_results->NextAs<IncrementDecrementOperator>();
  auto location = child_results->NextAs<Expression*>();
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
  auto name = child_results->NextAs<Identifier*>();
  if (!IsUpperCamelCase(name->value)) {
    NamingConventionError("Label", name, "UpperCamelCase");
  }
  auto types = child_results->NextAs<std::vector<TypeExpression*>>();
  return ParseResult{LabelAndTypes{name, std::move(types)}};
}

base::Optional<ParseResult> MakeNameAndType(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  auto type = child_results->NextAs<TypeExpression*>();
  return ParseResult{NameAndTypeExpression{name, type}};
}

base::Optional<ParseResult> MakeNameAndExpression(
    ParseResultIterator* child_results) {
  auto name = child_results->NextAs<Identifier*>();
  auto expression = child_results->NextAs<Expression*>();
  return ParseResult{NameAndExpression{name, expression}};
}

base::Optional<ParseResult> MakeNameAndExpressionFromExpression(
    ParseResultIterator* child_results) {
  auto expression = child_results->NextAs<Expression*>();
  if (auto* id = IdentifierExpression::DynamicCast(expression)) {
    if (!id->generic_arguments.empty() ||
        !id->namespace_qualification.empty()) {
      ReportError("expected a plain identifier without qualification");
    }
    return ParseResult{NameAndExpression{id->name, id}};
  }
  ReportError("Constructor parameters need to be named.");
}

base::Optional<ParseResult> MakeAnnotation(ParseResultIterator* child_results) {
  return ParseResult{
      Annotation{child_results->NextAs<Identifier*>(),
                 child_results->NextAs<base::Optional<std::string>>()}};
}

base::Optional<ParseResult> MakeClassField(ParseResultIterator* child_results) {
  AnnotationSet annotations(child_results, {"@noVerifier"}, {"@if", "@ifnot"});
  bool generate_verify = !annotations.Contains("@noVerifier");
  std::vector<ConditionalAnnotation> conditions;
  for (const std::string& condition : annotations.GetParams("@if")) {
    conditions.push_back({condition, ConditionalAnnotationType::kPositive});
  }
  for (const std::string& condition : annotations.GetParams("@ifnot")) {
    conditions.push_back({condition, ConditionalAnnotationType::kNegative});
  }
  auto weak = child_results->NextAs<bool>();
  auto const_qualified = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  auto index = child_results->NextAs<base::Optional<std::string>>();
  auto type = child_results->NextAs<TypeExpression*>();
  return ParseResult{ClassFieldExpression{{name, type},
                                          index,
                                          std::move(conditions),
                                          weak,
                                          const_qualified,
                                          generate_verify}};
}

base::Optional<ParseResult> MakeStructField(
    ParseResultIterator* child_results) {
  auto const_qualified = child_results->NextAs<bool>();
  auto name = child_results->NextAs<Identifier*>();
  auto type = child_results->NextAs<TypeExpression*>();
  return ParseResult{StructFieldExpression{{name, type}, const_qualified}};
}

base::Optional<ParseResult> ExtractAssignmentOperator(
    ParseResultIterator* child_results) {
  auto op = child_results->NextAs<Identifier*>();
  base::Optional<std::string> result =
      std::string(op->value.begin(), op->value.end() - 1);
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
    InputPosition current = *pos;
    MatchString("_", &current);
    if (!MatchChar(std::isalpha, &current)) return false;
    while (MatchChar(std::isalnum, &current) || MatchString("_", &current)) {
    }
    *pos = current;
    return true;
  }

  static bool MatchAnnotation(InputPosition* pos) {
    InputPosition current = *pos;
    if (!MatchString("@", &current)) return false;
    if (!MatchIdentifier(&current)) return false;
    *pos = current;
    return true;
  }

  static bool MatchIntrinsicName(InputPosition* pos) {
    InputPosition current = *pos;
    if (!MatchString("%", &current)) return false;
    if (!MatchIdentifier(&current)) return false;
    *pos = current;
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
  Symbol identifier = {Rule({Pattern(MatchIdentifier)}, YieldMatchedInput),
                       Rule({Token("runtime")}, YieldMatchedInput)};

  // Result: Identifier*
  Symbol name = {Rule({&identifier}, MakeIdentifier)};

  // Result: Identifier*
  Symbol annotationName = {
      Rule({Pattern(MatchAnnotation)}, MakeIdentifierFromMatchedInput)};

  // Result: std::string
  Symbol intrinsicName = {
      Rule({Pattern(MatchIntrinsicName)}, MakeIdentifierFromMatchedInput)};

  // Result: std::string
  Symbol stringLiteral = {
      Rule({Pattern(MatchStringLiteral)}, YieldMatchedInput)};

  // Result: std::string
  Symbol externalString = {Rule({&stringLiteral}, StringLiteralUnquoteAction)};

  // Result: std::string
  Symbol decimalLiteral = {
      Rule({Pattern(MatchDecimalLiteral)}, YieldMatchedInput),
      Rule({Pattern(MatchHexLiteral)}, YieldMatchedInput)};

  // Result: std::string
  Symbol annotationParameter = {Rule({&identifier}), Rule({&decimalLiteral}),
                                Rule({&externalString})};

  // Result: std::string
  Symbol annotationParameters = {
      Rule({Token("("), &annotationParameter, Token(")")})};

  // Result: Annotation
  Symbol annotation = {
      Rule({&annotationName, Optional<std::string>(&annotationParameters)},
           MakeAnnotation)};

  // Result: std::vector<Annotation>
  Symbol* annotations = List<Annotation>(&annotation);

  // Result: TypeList
  Symbol* typeList = List<TypeExpression*>(&type, Token(","));

  // Result: TypeExpression*
  Symbol simpleType = {
      Rule({Token("("), &type, Token(")")}),
      Rule({List<std::string>(Sequence({&identifier, Token("::")})),
            CheckIf(Token("constexpr")), &identifier,
            TryOrDefault<std::vector<TypeExpression*>>(
                &genericSpecializationTypeList)},
           MakeBasicTypeExpression),
      Rule({Token("builtin"), Token("("), typeList, Token(")"), Token("=>"),
            &simpleType},
           MakeFunctionTypeExpression),
      Rule({Token("&"), &simpleType}, MakeReferenceTypeExpression)};

  // Result: TypeExpression*
  Symbol type = {Rule({&simpleType}), Rule({&type, Token("|"), &simpleType},
                                           MakeUnionTypeExpression)};

  // Result: GenericParameters
  Symbol genericParameters = {
      Rule({Token("<"),
            List<Identifier*>(Sequence({&name, Token(":"), Token("type")}),
                              Token(",")),
            Token(">")})};

  // Result: TypeList
  Symbol genericSpecializationTypeList = {
      Rule({Token("<"), typeList, Token(">")})};

  // Result: base::Optional<TypeList>
  Symbol* optionalGenericParameters = Optional<TypeList>(&genericParameters);

  Symbol implicitParameterList{
      Rule({Token("("), OneOf({"implicit", "js-implicit"}),
            List<NameAndTypeExpression>(&nameAndType, Token(",")), Token(")")},
           MakeImplicitParameterList)};

  Symbol* optionalImplicitParameterList{
      Optional<ImplicitParameters>(&implicitParameterList)};

  // Result: ParameterList
  Symbol typeListMaybeVarArgs = {
      Rule({optionalImplicitParameterList, Token("("),
            List<TypeExpression*>(Sequence({&type, Token(",")})), Token("..."),
            Token(")")},
           MakeParameterList<true, false>),
      Rule({optionalImplicitParameterList, Token("("), typeList, Token(")")},
           MakeParameterList<false, false>)};

  // Result: LabelAndTypes
  Symbol labelParameter = {Rule(
      {&name,
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
  Symbol nameAndType = {Rule({&name, Token(":"), &type}, MakeNameAndType)};

  Symbol* optionalArraySpecifier =
      Optional<std::string>(Sequence({Token("["), &identifier, Token("]")}));

  Symbol classField = {
      Rule({annotations, CheckIf(Token("weak")), CheckIf(Token("const")), &name,
            optionalArraySpecifier, Token(":"), &type, Token(";")},
           MakeClassField)};

  Symbol structField = {
      Rule({CheckIf(Token("const")), &name, Token(":"), &type, Token(";")},
           MakeStructField)};

  // Result: ParameterList
  Symbol parameterListNoVararg = {
      Rule({optionalImplicitParameterList, Token("("),
            List<NameAndTypeExpression>(&nameAndType, Token(",")), Token(")")},
           MakeParameterList<false, true>)};

  // Result: ParameterList
  Symbol parameterListAllowVararg = {
      Rule({&parameterListNoVararg}),
      Rule({optionalImplicitParameterList, Token("("),
            List<NameAndTypeExpression>(Sequence({&nameAndType, Token(",")})),
            Token("..."), &identifier, Token(")")},
           MakeParameterList<true, true>)};

  // Result: Identifier*
  Symbol* OneOf(const std::vector<std::string>& alternatives) {
    Symbol* result = NewSymbol();
    for (const std::string& s : alternatives) {
      result->AddRule(Rule({Token(s)}, MakeIdentifierFromMatchedInput));
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

  // Result: Expression*
  Symbol identifierExpression = {
      Rule({List<std::string>(Sequence({&identifier, Token("::")})), &name,
            TryOrDefault<TypeList>(&genericSpecializationTypeList)},
           MakeIdentifierExpression),
  };

  // Result: std::vector<Expression*>
  Symbol argumentList = {Rule(
      {Token("("), List<Expression*>(expression, Token(",")), Token(")")})};

  // Result: Expression*
  Symbol callExpression = {Rule(
      {&identifierExpression, &argumentList, optionalOtherwise}, MakeCall)};

  // Result: Expression*
  Symbol callMethodExpression = {
      Rule({&primaryExpression, Token("."), &identifier, &argumentList,
            optionalOtherwise},
           MakeMethodCall)};

  // Result: NameAndExpression
  Symbol namedExpression = {
      Rule({&name, Token(":"), expression}, MakeNameAndExpression),
      Rule({expression}, MakeNameAndExpressionFromExpression)};

  // Result: std::vector<NameAndExpression>
  Symbol initializerList = {
      Rule({Token("{"), List<NameAndExpression>(&namedExpression, Token(",")),
            Token("}")})};

  // Result: Expression*
  Symbol intrinsicCallExpression = {Rule(
      {&intrinsicName, TryOrDefault<TypeList>(&genericSpecializationTypeList),
       &argumentList},
      MakeIntrinsicCallExpression)};

  // Result: Expression*
  Symbol primaryExpression = {
      Rule({&callExpression}),
      Rule({&callMethodExpression}),
      Rule({&intrinsicCallExpression}),
      Rule({&identifierExpression}),
      Rule({&primaryExpression, Token("."), &name}, MakeFieldAccessExpression),
      Rule({&primaryExpression, Token("["), expression, Token("]")},
           MakeElementAccessExpression),
      Rule({&decimalLiteral}, MakeNumberLiteralExpression),
      Rule({&stringLiteral}, MakeStringLiteralExpression),
      Rule({&simpleType, &initializerList}, MakeStructExpression),
      Rule({Token("new"), &simpleType, &initializerList}, MakeNewExpression),
      Rule({Token("("), expression, Token(")")})};

  // Result: Expression*
  Symbol unaryExpression = {
      Rule({&primaryExpression}),
      Rule({OneOf({"+", "-", "!", "~", "&"}), &unaryExpression},
           MakeUnaryOperator),
      Rule({Token("*"), &unaryExpression}, MakeDereferenceExpression),
      Rule({Token("..."), &unaryExpression}, MakeSpreadExpression),
      Rule({&incrementDecrementOperator, &unaryExpression},
           MakeIncrementDecrementExpressionPrefix),
      Rule({&unaryExpression, &incrementDecrementOperator},
           MakeIncrementDecrementExpressionPostfix)};

  // Result: Expression*
  Symbol* multiplicativeExpression =
      BinaryOperator(&unaryExpression, OneOf({"*", "/", "%"}));

  // Result: Expression*
  Symbol* additiveExpression =
      BinaryOperator(multiplicativeExpression, OneOf({"+", "-"}));

  // Result: Identifier*
  Symbol shiftOperator = {
      Rule({Token("<<")}, MakeIdentifierFromMatchedInput),
      Rule({Token(">"), Token(">")}, MakeRightShiftIdentifier),
      Rule({Token(">"), Token(">"), Token(">")}, MakeRightShiftIdentifier)};

  // Result: Expression*
  Symbol* shiftExpression = BinaryOperator(additiveExpression, &shiftOperator);

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
      Rule({&conditionalExpression, &assignmentOperator, &assignmentExpression},
           MakeAssignmentExpression)};

  // Result: Statement*
  Symbol block = {Rule({CheckIf(Token("deferred")), Token("{"),
                        List<Statement*>(&statement), Token("}")},
                       MakeBlockStatement)};

  // Result: LabelBlock*
  Symbol labelBlock = {
      Rule({Token("label"), &name,
            TryOrDefault<ParameterList>(&parameterListNoVararg), &block},
           MakeLabelBlock)};

  Symbol catchBlock = {
      Rule({Token("catch"), Token("("), &identifier, Token(")"), &block},
           MakeCatchBlock)};

  // Result: ExpressionWithSource
  Symbol expressionWithSource = {Rule({expression}, MakeExpressionWithSource)};

  Symbol* optionalTypeSpecifier =
      Optional<TypeExpression*>(Sequence({Token(":"), &type}));

  // Result: Statement*
  Symbol varDeclaration = {
      Rule({OneOf({"let", "const"}), &name, optionalTypeSpecifier},
           MakeVarDeclarationStatement)};

  // Result: Statement*
  Symbol varDeclarationWithInitialization = {
      Rule({OneOf({"let", "const"}), &name, optionalTypeSpecifier, Token("="),
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
      Rule({Token("goto"), &name,
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
      Rule({Token("try"), &block, List<LabelBlock*>(&labelBlock),
            Optional<LabelBlock*>(&catchBlock)},
           MakeTryLabelExpression),
      Rule({OneOf({"assert", "check"}), Token("("), &expressionWithSource,
            Token(")"), Token(";")},
           MakeAssertStatement),
      Rule({Token("while"), Token("("), expression, Token(")"), &statement},
           MakeWhileStatement),
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
  Symbol method = {Rule(
      {CheckIf(Token("transitioning")),
       Optional<std::string>(Sequence({Token("operator"), &externalString})),
       &name, &parameterListNoVararg, &optionalReturnType, optionalLabelList,
       &block},
      MakeMethodDeclaration)};

  // Result: std::vector<Declaration*>
  Symbol declaration = {
      Rule({Token("const"), &name, Token(":"), &type, Token("="), expression,
            Token(";")},
           AsSingletonVector<Declaration*, MakeConstDeclaration>()),
      Rule({Token("const"), &name, Token(":"), &type, Token("generates"),
            &externalString, Token(";")},
           AsSingletonVector<Declaration*, MakeExternConstDeclaration>()),
      Rule({annotations, CheckIf(Token("extern")), CheckIf(Token("transient")),
            Token("class"), &name,
            Optional<TypeExpression*>(Sequence({Token("extends"), &type})),
            Optional<std::string>(
                Sequence({Token("generates"), &externalString})),
            Token("{"), List<Declaration*>(&method),
            List<ClassFieldExpression>(&classField), Token("}")},
           AsSingletonVector<Declaration*, MakeClassDeclaration>()),
      Rule({Token("struct"), &name,
            TryOrDefault<GenericParameters>(&genericParameters), Token("{"),
            List<Declaration*>(&method),
            List<StructFieldExpression>(&structField), Token("}")},
           AsSingletonVector<Declaration*, MakeStructDeclaration>()),
      Rule({CheckIf(Token("transient")), Token("type"), &name,
            Optional<Identifier*>(Sequence({Token("extends"), &name})),
            Optional<std::string>(
                Sequence({Token("generates"), &externalString})),
            Optional<std::string>(
                Sequence({Token("constexpr"), &externalString})),
            Token(";")},
           MakeAbstractTypeDeclaration),
      Rule({Token("type"), &name, Token("="), &type, Token(";")},
           AsSingletonVector<Declaration*, MakeTypeAliasDeclaration>()),
      Rule({Token("intrinsic"), &intrinsicName,
            TryOrDefault<GenericParameters>(&genericParameters),
            &parameterListNoVararg, &optionalReturnType, &optionalBody},
           AsSingletonVector<Declaration*, MakeIntrinsicDeclaration>()),
      Rule({Token("extern"), CheckIf(Token("transitioning")),
            Optional<std::string>(
                Sequence({Token("operator"), &externalString})),
            Token("macro"),
            Optional<std::string>(Sequence({&identifier, Token("::")})), &name,
            TryOrDefault<GenericParameters>(&genericParameters),
            &typeListMaybeVarArgs, &optionalReturnType, optionalLabelList,
            Token(";")},
           AsSingletonVector<Declaration*, MakeExternalMacro>()),
      Rule({Token("extern"), CheckIf(Token("transitioning")),
            CheckIf(Token("javascript")), Token("builtin"), &name,
            TryOrDefault<GenericParameters>(&genericParameters),
            &typeListMaybeVarArgs, &optionalReturnType, Token(";")},
           AsSingletonVector<Declaration*, MakeExternalBuiltin>()),
      Rule({Token("extern"), CheckIf(Token("transitioning")), Token("runtime"),
            &name, &typeListMaybeVarArgs, &optionalReturnType, Token(";")},
           AsSingletonVector<Declaration*, MakeExternalRuntime>()),
      Rule({CheckIf(Token("@export")), CheckIf(Token("transitioning")),
            Optional<std::string>(
                Sequence({Token("operator"), &externalString})),
            Token("macro"), &name,
            TryOrDefault<GenericParameters>(&genericParameters),
            &parameterListNoVararg, &optionalReturnType, optionalLabelList,
            &optionalBody},
           AsSingletonVector<Declaration*, MakeTorqueMacroDeclaration>()),
      Rule({CheckIf(Token("transitioning")), CheckIf(Token("javascript")),
            Token("builtin"), &name,
            TryOrDefault<GenericParameters>(&genericParameters),
            &parameterListAllowVararg, &optionalReturnType, &optionalBody},
           AsSingletonVector<Declaration*, MakeTorqueBuiltinDeclaration>()),
      Rule({CheckIf(Token("transitioning")), &name,
            &genericSpecializationTypeList, &parameterListAllowVararg,
            &optionalReturnType, optionalLabelList, &block},
           AsSingletonVector<Declaration*, MakeSpecializationDeclaration>()),
      Rule({Token("#include"), &externalString},
           AsSingletonVector<Declaration*, MakeCppIncludeDeclaration>())};

  // Result: std::vector<Declaration*>
  Symbol declarationList = {
      Rule({List<std::vector<Declaration*>>(&declaration)}, ConcatList)};

  // Result: std::vector<Declaration*>
  Symbol namespaceDeclaration = {
      Rule({Token("namespace"), &identifier, Token("{"), &declarationList,
            Token("}")},
           AsSingletonVector<Declaration*, MakeNamespaceDeclaration>())};

  Symbol file = {Rule({&file, Token("import"), &externalString},
                      ProcessTorqueImportDeclaration),
                 Rule({&file, &namespaceDeclaration}, AddGlobalDeclarations),
                 Rule({&file, &declaration}, AddGlobalDeclarations), Rule({})};
};

}  // namespace

void ParseTorque(const std::string& input) {
  BuildFlags::Scope build_flags_scope;
  TorqueGrammar().Parse(input);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
