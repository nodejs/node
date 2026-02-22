// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/tsa-generator.h"

#include "src/torque/ast-visitor.h"
#include "src/torque/declaration-visitor.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/parameter-difference.h"
#include "src/torque/type-inference.h"
#include "src/torque/type-visitor.h"

namespace v8::internal::torque {

struct TSAValue {
  const Type* type;
};

template <>
inline std::string Binding<TSAValue>::BindingTypeString() const {
  return "TSAValue ";
}
template <>
inline bool Binding<TSAValue>::CheckWritten() const {
  return false;
}

class TSAGenerator : public AstVisitor<TSAGenerator> {
  static constexpr size_t kSpacesPerIndentationLevel = 2;

  DECLARE_CONTEXTUAL_VARIABLE(ValueBindingsManager, BindingsManager<TSAValue>);
  struct TargetBase {
    const Type* return_type = nullptr;
    std::string return_label_name;
  };

  struct MacroTarget : TargetBase {};

  struct BuiltinTarget : TargetBase {};

  struct PerSourceData {
    std::stringstream h_stream;
    std::stringstream cc_stream;
    SourceId source;

    explicit PerSourceData(SourceId source) : source(source) {}
  };
  struct IndentationScope {
    explicit IndentationScope(TSAGenerator* generator) : generator_(generator) {
      generator_->IncreaseIndentation();
    }
    ~IndentationScope() { generator_->DecreaseIndentation(); }

    TSAGenerator* generator_;
  };

  struct OutputBufferScope {
    explicit OutputBufferScope(TSAGenerator* generator)
        : generator_(generator) {
      previous_stream_ = generator_->BeginOutputToBuffer(&buffer_stream_);
    }

    ~OutputBufferScope() {
      generator_->EndOutputToBuffer(&buffer_stream_, previous_stream_);
    }

    std::stringstream& buffer() { return buffer_stream_; }
    std::string ToString() const { return buffer_stream_.str(); }

    TSAGenerator* generator_;
    std::stringstream buffer_stream_;
    std::stringstream* previous_stream_ = nullptr;
  };

  template <typename T>
  class BlockBindings : public torque::BlockBindings<T> {
   public:
    BlockBindings(TSAGenerator* generator, BindingsManager<T>* manager)
        : torque::BlockBindings<T>(manager), generator_(generator) {
      generator_->EnterBlockBindingsScope(this);
    }

    ~BlockBindings() { generator_->LeaveBlockBindingsScope(this); }

   private:
    TSAGenerator* generator_;
  };

  // The `reducer_name` out param is without the `TorqueGenerated` prefix and
  // without the `Reducer` suffix. The assembler name (if builtins are
  // generated) will be `reducer_name` suffixed with `AssemblerTS`.
  // E.g. a `reducer_name` of `StringBuiltins`
  // will trigger the generation of a `TorqueGeneratedStringBuiltinsReducer` and
  // a `StringBuiltinsAssemblerTS` (if necessary).
  bool SourceShouldGenerateReducer(SourceId source, std::string& reducer_name) {
    std::string name = SourceFileMap::PathFromV8RootWithoutExtension(source);
    if (!name.starts_with("src/builtins/")) return false;
    name = name.substr(strlen("src/builtins/"));

    // For some reason, string and bigint files have a non-consistent name.
    if (name == "builtins-string") {
      reducer_name = "StringBuiltins";
      return true;
    } else if (name == "builtins-bigint") {
      reducer_name = "BigintBuiltins";
      return true;
    }

    reducer_name = CamelifyString(name);
    return true;
  }

 public:
  void Run(Ast& ast, const std::string& output_directory) {
    for (SourceId source : SourceFileMap::AllSources()) {
      per_source_data_.emplace(source, source);
      std::string reducer_name;
      const bool emit_reducer =
          SourceShouldGenerateReducer(source, reducer_name);

      // Header file.
      {
        BeginOutputToHFile(source, 0);

        // Include guard.
        const std::string header_define =
            "V8_GEN_TORQUE_GENERATED_" +
            UnderlinifyPath(SourceFileMap::PathFromV8Root(source)) + "_TSA_H_";
        out() << "#ifndef " << header_define << "\n"
              << "#define " << header_define << "\n\n"
              << "#include "
                 "\"src/codegen/turboshaft-builtins-assembler-inl.h\"\n\n";

        // Open namespace.
        out() << "namespace v8::internal {\n\n";

        out() << "#include "
                 "\"src/compiler/turboshaft/define-assembler-macros.inc\"\n\n";

        // Start this file's assembler.
        if (emit_reducer) {
#ifdef DEBUG
          std::cout << "Torque: Generating TorqueGenerated" << reducer_name
                    << "Reducer from " << SourceFileMap::PathFromV8Root(source)
                    << ".\n";
#endif

          out() << "template <typename Next>\n"
                   "class TorqueGenerated"
                << reducer_name
                << "Reducer : public Next {\n"
                   " public:\n"
                   "  BUILTIN_REDUCER(TorqueGenerated"
                << reducer_name << ")\n\n";
        }

        EndOutputToFile();
      }

      // Source file.
      {
        BeginOutputToCCFile(source, 0);

        // Include Turboshaft's builtin assembler.
        out() << "#include \"src/builtins/builtins-utils-gen.h\"\n\n";

        // Emit all explicit cpp includes of source file. This will also include
        // CSA related files for now, but it's an acceptable temporary solution.
        for (Declaration* decl : ast.declarations()) {
          if (const CppIncludeDeclaration* incl =
                  CppIncludeDeclaration::DynamicCast(decl)) {
            if (incl->pos.source == source) {
              if (incl->include_selector == IncludeSelector::kTSA ||
                  incl->include_selector == IncludeSelector::kAny) {
                out() << "#include \"" << incl->include_path << "\"\n";
              }
            }
          }
        }

        // Include own header.
        out() << "\n#include \"torque-generated/" +
                     SourceFileMap::PathFromV8RootWithoutExtension(source) +
                     "-tq-tsa.h\"\n\n";

        // Open namespace.
        out() << "namespace v8::internal {\n\n";

        // // TODO(nicohartmann): Currently we need to pull in turboshaft
        // namespace. Reconsider this.
        out() << "using namespace compiler::turboshaft;\n\n";

        EndOutputToFile();
      }
    }

    // Emit everything.
    const std::vector<std::unique_ptr<Declarable>>& all_declarables =
        GlobalContext::AllDeclarables();
    for (const std::unique_ptr<Declarable>& declarable : all_declarables) {
      SourceId source = declarable->Position().source;
      CurrentScope::Scope current_scope(declarable->ParentScope());
      switch (declarable->kind()) {
        case Declarable::kTorqueMacro:
          GenerateMacro(TorqueMacro::cast(declarable.get()), source);
          break;
        case Declarable::kBuiltin:
          GenerateBuiltin(Builtin::cast(declarable.get()), source);
          break;
        default:
          // Nothing to do here for now.
          break;
      }
    }

    for (SourceId source : SourceFileMap::AllSources()) {
      std::string reducer_name;
      const bool emit_reducer =
          SourceShouldGenerateReducer(source, reducer_name);

      // Header file.
      {
        BeginOutputToHFile(source, 0);

        const std::string header_define =
            "V8_GEN_TORQUE_GENERATED_" +
            UnderlinifyPath(SourceFileMap::PathFromV8Root(source)) + "_TSA_H_";
        if (emit_reducer) {
          // Close reducer.
          out() << "};\n\n";
        }

        out() << "#include "
                 "\"src/compiler/turboshaft/undef-assembler-macros.inc\"\n\n";

        // Close namespace
        out() << "}  // namespace v8::internal\n\n";

        // End include guard.
        out() << "#endif  // " << header_define << "\n";

        EndOutputToFile();
      }

      // Source file.
      {
        BeginOutputToCCFile(source, 0);

        // Close namespace.
        out() << "}  // namespace v8::internal\n";

        EndOutputToFile();
      }
    }

    for (SourceId source : SourceFileMap::AllSources()) {
      std::string base_filename =
          output_directory + "/" +
          SourceFileMap::PathFromV8RootWithoutExtension(source);

      PerSourceData& data = GetPerSourceData(source);
      WriteFile(base_filename + "-tq-tsa.cc", data.cc_stream.str());
      WriteFile(base_filename + "-tq-tsa.h", data.h_stream.str());
    }
  }

 private:
  friend class AstVisitor<TSAGenerator>;
  enum class OutputFile { kNone, kH, kCC };

  void GenerateMacro(TorqueMacro* macro, SourceId source) {
    if (!macro->SupportsTSA()) return;

    DCHECK(std::holds_alternative<std::nullptr_t>(target_));

    const Signature& signature = macro->signature();
    const Type* return_type = signature.return_type;

    BeginOutputToHFile(source, 1);
    target_ = MacroTarget{
        {.return_type = return_type, .return_label_name = "_return"}};

    CurrentSourcePosition::Scope scource_position(macro->Position());
    ValueBindingsManager::Scope binding_scope;

    if (return_type->IsVoidOrNever()) {
      out_i() << "void ";
    } else {
      out_i() << "V<" << return_type->GetGeneratedTNodeTypeName() << "> ";
    }
    out() << macro->ReadableName() << "(";

    BlockBindings<TSAValue> macro_bindings(this, &ValueBindingsManager::Get());
    size_t param_index = 0;
    // TODO(nicohartmann): Handle methods and `this` argument.
    for (; param_index < signature.parameter_types.types.size();
         ++param_index) {
      if (param_index != 0) out() << ", ";
      // TODO(nicohartmann): Could make some ConstOrV<> maybe.
      const Identifier* name = signature.parameter_names[param_index];
      const Type* type = signature.parameter_types.types[param_index];
      out() << "V<" << type->GetGeneratedTNodeTypeName() << "> " << name->value;
      macro_bindings.Add(name, TSAValue{.type = type});
    }

    out() << ") {\n";

    // Generate the body.
    {
      IndentationScope indent(this);

      // If we have a return value, we need to set up the return label.
      out_i() << "Label<";
      if (!return_type->IsVoidOrNever()) {
        out() << return_type->GetGeneratedTNodeTypeName();
      }
      out() << "> _return(this);\n\n";

      auto macro_body = BlockStatement::DynamicCast(macro->body().value());
      DCHECK_NOT_NULL(macro_body);
      VisitBlockStatement(macro_body, false);

      // Bind the return label and return the value (if any).
      out() << "\n";
      if (return_type->IsVoidOrNever()) {
        out_i() << "BIND(_return);\n";
      } else {
        out_i() << "BIND(_return, return_value);\n";
        out_i() << "return return_value;\n";
      }
    }

    out_i() << "}\n\n";

    target_ = nullptr;
    EndOutputToFile();
  }

  void GenerateBuiltin(Builtin* builtin, SourceId source) {
    if (!builtin->SupportsTSA()) return;

    std::string reducer_name;
    CHECK(SourceShouldGenerateReducer(source, reducer_name));

    DCHECK(std::holds_alternative<std::nullptr_t>(target_));

    const Signature& signature = builtin->signature();
    const Type* return_type = signature.return_type;

    BeginOutputToCCFile(source, 0);
    target_ =
        BuiltinTarget{{.return_type = return_type,
                       // TODO(nicohartmann): See how we support returns here.
                       .return_label_name = ""}};

    CurrentSourcePosition::Scope scource_position(builtin->Position());
    ValueBindingsManager::Scope binding_scope;

    out_i() << "TS_BUILTIN(" << builtin->ExternalName() << ", " << reducer_name
            << "AssemblerTS) {\n";

    {
      IndentationScope indent(this);
      BlockBindings<TSAValue> builtin_bindings(this,
                                               &ValueBindingsManager::Get());

      size_t param_index = 0;
      for (; param_index < signature.parameter_types.types.size();
           ++param_index) {
        const Identifier* name = signature.parameter_names[param_index];
        const Type* type = signature.parameter_types.types[param_index];
        out_i() << "auto " << name->value << " = Parameter<"
                << type->GetGeneratedTNodeTypeName() << ">(Descriptor::k"
                << CamelifyString(name->value) << ");\n";
        builtin_bindings.Add(name, TSAValue{.type = type});
      }

      auto builtin_body = BlockStatement::DynamicCast(builtin->body().value());
      DCHECK_NOT_NULL(builtin_body);
      VisitBlockStatement(builtin_body, false);

#if 0
      // Emit parameters.
      const ParameterList& params = node->parameters;
      for (size_t i = 0; i < params.types.size(); ++i) {
        out() << "  auto " << params.names[i]->value << " = Parameter<";
        Visit(params.types[i]);
        out() << ">(Descriptor::k" << CamelifyString(params.names[i]->value)
              << ");\n";
      }

      DCHECK(node->body.has_value());
      // If we have a block statement, we unwrap the content since we already
      // emitted the braces.
      if (BlockStatement* block =
              BlockStatement::DynamicCast(node->body.value())) {
        Visit(block->statements);
      } else {
        Visit(node->body.value());
      }
#endif
    }
    out_i() << "}\n\n";

    target_ = nullptr;
    EndOutputToFile();
  }

  void FailCallableLookup(
      const std::string& reason, const QualifiedName& name,
      const TypeVector& parameter_types,
      const std::vector<Binding<LocalLabel>*>& labels,
      const std::vector<Signature>& candidates,
      const std::vector<std::pair<GenericCallable*, std::string>>
          inapplicable_generics) {
    std::stringstream stream;
    stream << "\n"
           << reason << ": \n  " << name << "(" << parameter_types << ")";
    if (!labels.empty()) {
      stream << " labels ";
      for (size_t i = 0; i < labels.size(); ++i) {
        stream << labels[i]->name() << "(" << labels[i]->parameter_types << ")";
      }
    }
    stream << "\ncandidates are:";
    for (const Signature& signature : candidates) {
      stream << "\n  " << name;
      PrintSignature(stream, signature, false);
    }
    if (!inapplicable_generics.empty()) {
      stream << "\nfailed to instantiate all of these generic declarations:";
      for (auto& failure : inapplicable_generics) {
        GenericCallable* generic = failure.first;
        const std::string& fail_reason = failure.second;
        stream << "\n  " << generic->name() << " defined at "
               << PositionAsString(generic->Position()) << ":\n    "
               << fail_reason << "\n";
      }
    }
    ReportError(stream.str());
  }

  TypeArgumentInference InferSpecializationTypes(
      GenericCallable* generic, const TypeVector& explicit_specialization_types,
      const TypeVector& explicit_arguments) {
    std::vector<std::optional<const Type*>> all_arguments;
    const ParameterList& parameters = generic->declaration()->parameters;
    for (size_t i = 0; i < parameters.implicit_count; ++i) {
      // TODO(nicohartmann): Currently we just use the parameter type of the
      // declaration, but we might need to consider specialization types.
      const Type* type = TypeVisitor::ComputeType(parameters.types[i]);
      all_arguments.push_back(type);
#if 0
      std::optional<Binding<LocalValue>*> val =
          TryLookupLocalValue(parameters.names[i]->value);
      all_arguments.push_back(
          val ? (*val)->GetLocationReference(*val).ReferencedType()
              : std::nullopt);
#endif
    }
    for (const Type* explicit_argument : explicit_arguments) {
      all_arguments.push_back(explicit_argument);
    }
    return generic->InferSpecializationTypes(explicit_specialization_types,
                                             all_arguments);
  }

  template <class Container>
  Callable* LookupCallable(const QualifiedName& name,
                           const Container& declaration_container,
                           const TypeVector& parameter_types,
                           const std::vector<Binding<LocalLabel>*>& labels,
                           const TypeVector& specialization_types,
                           bool silence_errors = false) {
    Callable* result = nullptr;

    std::vector<Declarable*> overloads;
    std::vector<Signature> overload_signatures;
    std::vector<std::pair<GenericCallable*, std::string>> inapplicable_generics;
    for (auto* declarable : declaration_container) {
      if (GenericCallable* generic = GenericCallable::DynamicCast(declarable)) {
        TypeArgumentInference inference = InferSpecializationTypes(
            generic, specialization_types, parameter_types);
        if (inference.HasFailed()) {
          inapplicable_generics.push_back(
              std::make_pair(generic, inference.GetFailureReason()));
          continue;
        }
        overloads.push_back(generic);
        overload_signatures.push_back(
            DeclarationVisitor::MakeSpecializedSignature(
                SpecializationKey<GenericCallable>{generic,
                                                   inference.GetResult()}));
      } else if (Callable* callable = Callable::DynamicCast(declarable)) {
        overloads.push_back(callable);
        overload_signatures.push_back(callable->signature());
      }
    }
    // Indices of candidates in overloads/overload_signatures.
    std::vector<size_t> candidates;
    for (size_t i = 0; i < overloads.size(); ++i) {
      const Signature& signature = overload_signatures[i];
      if (IsCompatibleSignature(signature, parameter_types, labels.size())) {
        candidates.push_back(i);
      }
    }

    if (overloads.empty() && inapplicable_generics.empty()) {
      if (silence_errors) return nullptr;
      std::stringstream stream;
      stream << "no matching declaration found for " << name;
      ReportError(stream.str());
    } else if (candidates.empty()) {
      if (silence_errors) return nullptr;
      FailCallableLookup("cannot find suitable callable with name", name,
                         parameter_types, labels, overload_signatures,
                         inapplicable_generics);
    }

    auto is_better_candidate = [&](size_t a, size_t b) {
      return ParameterDifference(overload_signatures[a].GetExplicitTypes(),
                                 parameter_types)
          .StrictlyBetterThan(ParameterDifference(
              overload_signatures[b].GetExplicitTypes(), parameter_types));
    };

    size_t best = *std::min_element(candidates.begin(), candidates.end(),
                                    is_better_candidate);
    // This check is contained in libstdc++'s std::min_element.
    DCHECK(!is_better_candidate(best, best));
    for (size_t candidate : candidates) {
      if (candidate != best && !is_better_candidate(best, candidate)) {
        std::vector<Signature> candidate_signatures;
        candidate_signatures.reserve(candidates.size());
        for (size_t i : candidates) {
          candidate_signatures.push_back(overload_signatures[i]);
        }
        FailCallableLookup("ambiguous callable ", name, parameter_types, labels,
                           candidate_signatures, inapplicable_generics);
      }
    }

    if (GenericCallable* generic =
            GenericCallable::DynamicCast(overloads[best])) {
      UNIMPLEMENTED();
      USE(generic);
#if 0
      TypeArgumentInference inference = InferSpecializationTypes(
          generic, specialization_types, parameter_types);
      result = GetOrCreateSpecialization(
          SpecializationKey<GenericCallable>{generic, inference.GetResult()});
#endif
    } else {
      result = Callable::cast(overloads[best]);
    }

    size_t caller_size = parameter_types.size();
    size_t callee_size =
        result->signature().types().size() - result->signature().implicit_count;
    if (caller_size != callee_size &&
        !result->signature().parameter_types.var_args) {
      std::stringstream stream;
      stream << "parameter count mismatch calling " << *result << " - expected "
             << std::to_string(callee_size) << ", found "
             << std::to_string(caller_size);
      ReportError(stream.str());
    }

    return result;
  }

  // TODO(nicohartmann): Remove those once everything is implemented in
  // VisitExpression_xyz and AstVisitor base class is replaced.
  AstNode* VisitCallExpression(CallExpression* node) { UNREACHABLE(); }

  AstNode* VisitCallMethodExpression(CallMethodExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitIntrinsicCallExpression(IntrinsicCallExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitStructExpression(StructExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitLogicalOrExpression(LogicalOrExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitLogicalAndExpression(LogicalAndExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitSpreadExpression(SpreadExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitConditionalExpression(ConditionalExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }

  AstNode* VisitIdentifierExpression(IdentifierExpression* node) {
    for (const std::string& nq : node->namespace_qualification) {
      out() << nq << "::";
    }
    out() << node->name->value;
    // TODO(nicohartmann): Support generic arguments.
    DCHECK(node->generic_arguments.empty());
    return node;
  }

  AstNode* VisitStringLiteralExpression(StringLiteralExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitIntegerLiteralExpression(IntegerLiteralExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitFloatingPointLiteralExpression(
      FloatingPointLiteralExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitFieldAccessExpression(FieldAccessExpression* node) {
    UNREACHABLE();
  }
  AstNode* VisitElementAccessExpression(ElementAccessExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitDereferenceExpression(DereferenceExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitAssignmentExpression(AssignmentExpression* node) {
    UNREACHABLE();
  }
  AstNode* VisitIncrementDecrementExpression(
      IncrementDecrementExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitNewExpression(NewExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitAssumeTypeImpossibleExpression(
      AssumeTypeImpossibleExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitStatementExpression(StatementExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }
  AstNode* VisitTryLabelExpression(TryLabelExpression* node) {
    // TODO(nicohartmann): Implement.
    UNIMPLEMENTED();
  }

  AstNode* VisitBasicTypeExpression(BasicTypeExpression* node) {
    // TODO(nicohartmann): Support these.
    DCHECK(node->namespace_qualification.empty());
    DCHECK(node->generic_arguments.empty());
    DCHECK(!node->is_constexpr);
    out() << node->name->value;

    return node;
  }

  AstNode* VisitFunctionTypeExpression(FunctionTypeExpression* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitPrecomputedTypeExpression(PrecomputedTypeExpression* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitUnionTypeExpression(UnionTypeExpression* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }

  AstNode* VisitBlockStatement(BlockStatement* node, bool emit_braces = true) {
    if (emit_braces) {
      out() << "{\n";
      {
        IndentationScope indent(this);
        Visit(node->statements);
      }
      out_i() << "}\n";
    } else {
      // We don't do extra indentation here, since we assume caller did this
      // already.
      Visit(node->statements);
    }
    return node;
  }

  AstNode* VisitExpressionStatement(ExpressionStatement* node) {
    out_i();
    VisitExpression(node->expression);
    out() << ";\n";
    return node;
  }

  AstNode* VisitIfStatement(IfStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }

  AstNode* VisitWhileStatement(WhileStatement* node) {
    out_i() << "WHILE(";
    Visit(node->condition);
    out() << ") ";
    Visit(node->body);
    return node;
  }

  AstNode* VisitTypeswitchStatement(TypeswitchStatement* node) {
    out_i() << "TYPESWITCH(";
    Visit(node->expr);
    out() << ") {\n";

    // Emit cases.
    {
      IndentationScope indent(this);
      for (const TypeswitchCase& c : node->cases) {
        out_i() << "CASE_(V<";
        Visit(c.type);
        out() << ">, ";
        BlockBindings<TSAValue> case_bindings(this,
                                              &ValueBindingsManager::Get());
        if (c.name.has_value()) {
          out() << c.name.value()->value;
          const Type* type = TypeVisitor::ComputeType(c.type);
          case_bindings.Add(c.name.value()->value, {.type = type});
        } else {
          out() << "_";
        }
        out() << "): ";
        Visit(c.block);
      }
    }

    out_i() << "}\n";
    return node;
  }

  AstNode* VisitForLoopStatement(ForLoopStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitBreakStatement(BreakStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitContinueStatement(ContinueStatement* node) {
    out_i() << "CONTINUE;\n";
    return node;
  }

  AstNode* VisitReturnStatement(ReturnStatement* node) {
    if (MacroTarget* macro = target<MacroTarget>()) {
      // In a macro we cannot use return. We need to jump to the exit label.
      // TODO(nicohartmann): Handle simple cases where this can be avoided.
      out_i() << "GOTO(" << macro->return_label_name;
      if (!macro->return_type->IsVoidOrNever()) {
        out() << ", ";
        VisitExpression(node->value.value());
      }
      out() << ");\n";
    } else if (target<BuiltinTarget>()) {
      out_i() << "Return(";
      if (node->value.has_value()) {
        VisitExpression(node->value.value());
      }
      out() << ");\n";
    } else {
      UNREACHABLE();
    }

    return node;
  }

  AstNode* VisitDebugStatement(DebugStatement* node) {
    switch (node->kind) {
      case DebugStatement::Kind::kUnreachable:
        out_i() << __() << "Unreachable();\n";
        break;
      case DebugStatement::Kind::kDebug:
        UNIMPLEMENTED();
    }
    return node;
  }
  AstNode* VisitAssertStatement(AssertStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitTailCallStatement(TailCallStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }

  AstNode* VisitVarDeclarationStatement(VarDeclarationStatement* node) {
    if (node->const_qualified) {
      // TODO(nicohartmann): Support const qualified declarations.
      UNIMPLEMENTED();
    } else {
      const Type* type = nullptr;
      if (node->type.has_value()) {
        out_i() << "ScopedVar<";
        // TODO(nicohartmann): Not sure if visiting the type is the best thing
        // to do here.
        Visit(node->type.value());
        type = TypeVisitor::ComputeType(node->type.value());
        out() << "> ";
      } else {
        // Should use computed type of initializer.
        UNIMPLEMENTED();
      }

      out() << node->name->value << "(this";
      if (node->initializer.has_value()) {
        out() << ", ";
        VisitExpression(node->initializer.value());
      }
      out() << ");\n";
      current_block_bindings()->Add(node->name, {.type = type});
    }
    return node;
  }

  AstNode* VisitGotoStatement(GotoStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitAbstractTypeDeclaration(AbstractTypeDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitTypeAliasDeclaration(TypeAliasDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitBitFieldStructDeclaration(BitFieldStructDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitClassDeclaration(ClassDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitStructDeclaration(StructDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitGenericCallableDeclaration(GenericCallableDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitGenericTypeDeclaration(GenericTypeDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitSpecializationDeclaration(SpecializationDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitExternConstDeclaration(ExternConstDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }

  AstNode* VisitNamespaceDeclaration(NamespaceDeclaration* node) {
    // For now, just visit all declarations in the namespace.
    return AstVisitor::VisitNamespaceDeclaration(node);
  }

  AstNode* VisitConstDeclaration(ConstDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitCppIncludeDeclaration(CppIncludeDeclaration* node) {
    // Nothing to do here.
    return node;
  }

  AstNode* VisitTorqueMacroDeclaration(TorqueMacroDeclaration* node) {
    UNREACHABLE();
  }

  AstNode* VisitTorqueBuiltinDeclaration(TorqueBuiltinDeclaration* node) {
    UNREACHABLE();
  }

  AstNode* VisitExternalMacroDeclaration(ExternalMacroDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitExternalBuiltinDeclaration(ExternalBuiltinDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitExternalRuntimeDeclaration(ExternalRuntimeDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitIntrinsicDeclaration(IntrinsicDeclaration* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitIdentifier(Identifier* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitTryHandler(TryHandler* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }
  AstNode* VisitClassBody(ClassBody* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }

  struct ExpressionResult {
    const Type* type;

    explicit ExpressionResult(const Type* type) : type(type) {}
  };

  ExpressionResult VisitExpression(Expression* expr) {
    switch (expr->kind) {
#define EXPR_CASE(kind)        \
  case AstNode::Kind::k##kind: \
    return VisitExpression_##kind(kind::cast(expr));
      AST_EXPRESSION_NODE_KIND_LIST(EXPR_CASE)
#undef EXPR_CASE
      default:
        UNREACHABLE();
    }
  }

  ExpressionResult VisitExpression_CallExpression(CallExpression* expr) {
    if (expr->callee->name->value == "&" && expr->arguments.size() == 1) {
      UNIMPLEMENTED();
#if 0
      if (auto* loc_expr
            = LocationExpression::DynamicCast(expr->arguments[0])) {
        LocationReference ref = GetLocationReference(loc_expr);
        if (ref.IsHeapReference()) return scope.Yield(ref.heap_reference());
        if (ref.IsHeapSlice()) return scope.Yield(ref.heap_slice());
      }
      ReportError("Unable to create a heap reference.");
#endif
    }

    QualifiedName name = QualifiedName(expr->callee->namespace_qualification,
                                       expr->callee->name->value);
    TypeVector specialization_types =
        TypeVisitor::ComputeTypeVector(expr->callee->generic_arguments);
    bool has_template_arguments = !specialization_types.empty();
    // TODO(nicohartmann): Support template arguments.
    CHECK(!has_template_arguments);

    TypeVector argument_types;
    std::vector<std::string> argument_strings;
    for (Expression* arg : expr->arguments) {
      OutputBufferScope output_buffer(this);
      ExpressionResult arg_result = VisitExpression(arg);
      DCHECK_NOT_NULL(arg_result.type);
      argument_types.push_back(arg_result.type);
      argument_strings.push_back(output_buffer.ToString());
    }

    Callable* callable =
        LookupCallable(name, Declarations::Lookup(name), argument_types,
                       /*labels: */ {}, specialization_types);

    if (RuntimeFunction* runtime_function =
            RuntimeFunction::DynamicCast(callable)) {
      runtime_function->ExternalName();
      out() << __(kTemplate)
            << "CallRuntime<runtime::" << runtime_function->ExternalName()
            << ">(";
      if (runtime_function->signature().HasContextParameter()) {
        out() << "context, ";
      }
      // TODO(nicohartmann): We currently need the base class initializer list
      // here because of the `ArgumentsBase` fix in call descriptors. Once this
      // is gone (currently blocked on an older gcc version), we can remove
      // this.
      out() << "{{}";
      for (const std::string& arg : argument_strings) {
        // Skip the context argument if we pass this explicitly.
        if (arg == "context") continue;
        out() << ", " << arg;
      }
      out() << "})";
    } else {
      out() << __() << expr->callee->name << "(";
      bool first = true;
      for (size_t i = 0; i < callable->signature().implicit_count; ++i) {
        std::string implicit_name =
            callable->signature().parameter_names[i]->value;
        if (!first) out() << ", ";
        out() << implicit_name;
        first = false;
      }
      DCHECK_EQ(expr->arguments.size(), argument_strings.size());
      for (size_t i = 0; i < argument_strings.size(); ++i) {
        if (!first) out() << ", ";
        out() << argument_strings[i];
        first = false;
      }
      // TODO(nicohartmann): Support labels.
      DCHECK(expr->labels.empty());
      out() << ")";
    }
    return ExpressionResult(callable->signature().return_type);

#if 0
    arguments.labels = LabelsFromIdentifiers(expr->labels);
    if (!has_template_arguments && name.namespace_qualification.empty() &&
        TryLookupLocalValue(name.name)) {
      return scope.Yield(
          GeneratePointerCall(expr->callee, arguments, is_tailcall));
    } else {
      if (expr->callee->name->value == "!"
          && arguments.parameters.size() == 1) {
        PropagateBitfieldMark(expr->arguments[0], expr);
      }
      if (expr->callee->name->value == "=="
          && arguments.parameters.size() == 2) {
        if (arguments.parameters[0].type()->IsConstexpr()) {
          PropagateBitfieldMark(expr->arguments[1], expr);
        } else if (arguments.parameters[1].type()->IsConstexpr()) {
          PropagateBitfieldMark(expr->arguments[0], expr);
        }
      }
      return scope.Yield(
          GenerateCall(name, arguments, specialization_types, is_tailcall));
    }
#endif
  }

  ExpressionResult VisitExpression_CallMethodExpression(
      CallMethodExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_IntrinsicCallExpression(
      IntrinsicCallExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_StructExpression(StructExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_LogicalOrExpression(
      LogicalOrExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_LogicalAndExpression(
      LogicalAndExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_SpreadExpression(SpreadExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_ConditionalExpression(
      ConditionalExpression* expr) {
    UNIMPLEMENTED();
  }

  std::optional<Binding<TSAValue>*> TryLookupLocalValue(
      const std::string& name) {
    return ValueBindingsManager::Get().TryLookup(name);
  }

  ExpressionResult VisitExpression_IdentifierExpression(
      IdentifierExpression* expr) {
    if (expr->namespace_qualification.empty()) {
      if (std::optional<Binding<TSAValue>*> value =
              TryLookupLocalValue(expr->name->value)) {
        if (!expr->generic_arguments.empty()) {
          ReportError("cannot have generic parameters on local name ",
                      expr->name);
        }
        out() << expr->name->value;
        return ExpressionResult{(*value)->type};
      }
    }

    DCHECK(!expr->IsThis());

    QualifiedName name =
        QualifiedName(expr->namespace_qualification, expr->name->value);
    if (std::optional<Builtin*> builtin =
            Declarations::TryLookupBuiltin(name)) {
      UNIMPLEMENTED();
#if 0
      // TODO(v8:12261): Consider collecting KytheData here.
      return LocationReference::Temporary(GetBuiltinCode(*builtin),
                                          "builtin " + expr->name->value);
#endif
    }
    if (!expr->generic_arguments.empty()) {
      UNIMPLEMENTED();
#if 0
      GenericCallable* generic = Declarations::LookupUniqueGeneric(name);
      Callable* specialization =
          GetOrCreateSpecialization(SpecializationKey<GenericCallable>{
              generic, TypeVisitor::ComputeTypeVector(
                expr->generic_arguments)});
      if (Builtin* builtin = Builtin::DynamicCast(specialization)) {
        DCHECK(!builtin->IsExternal());
        return LocationReference::Temporary(GetBuiltinCode(builtin),
                                            "builtin " + expr->name->value);
      } else {
        ReportError("cannot create function pointer for non-builtin ",
                    generic->name());
      }
#endif
    }
    Value* value = Declarations::LookupValue(name);
    CHECK(value->Position().source.IsValid());
    // TODO(nicohartmann): Do we need this?
#if 0
    if (auto stream = CurrentFileStreams::Get()) {
      stream->required_builtin_includes.insert(value->Position().source);
    }
#endif
    if (auto* constant = NamespaceConstant::DynamicCast(value)) {
      if (constant->type()->IsConstexpr()) {
        out() << constant->external_name();
        return ExpressionResult(constant->type());
#if 0
        return LocationReference::Temporary(
            VisitResult(constant->type(),
              constant->external_name() + "(state_)"),
            "namespace constant " + expr->name->value);
#endif
      }
      UNIMPLEMENTED();
#if 0
      assembler().Emit(NamespaceConstantInstruction{constant});
      StackRange stack_range =
          assembler().TopRange(LoweredSlotCount(constant->type()));
      return LocationReference::Temporary(
          VisitResult(constant->type(), stack_range),
          "namespace constant " + expr->name->value);
#endif
    }
    ExternConstant* constant = ExternConstant::cast(value);
    out() << name;
    return ExpressionResult(constant->type());
  }
  ExpressionResult VisitExpression_StringLiteralExpression(
      StringLiteralExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_IntegerLiteralExpression(
      IntegerLiteralExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_FloatingPointLiteralExpression(
      FloatingPointLiteralExpression* expr) {
    UNIMPLEMENTED();
  }

  ExpressionResult VisitExpression_FieldAccessExpression(
      FieldAccessExpression* expr) {
    // This is a load. Store will be handled in VisitLocation.
    out() << __() << "LoadField(";
    ExpressionResult object = VisitExpression(expr->object);
    out() << ", FIELD(" << object.type->GetGeneratedTNodeTypeName() << ", "
          << expr->field->value << "_))";
    if (auto class_type = object.type->ClassSupertype()) {
      const Field& field = (*class_type)->LookupField(expr->field->value);
      return ExpressionResult(field.name_and_type.type);
    } else {
      UNIMPLEMENTED();
    }
  }

  ExpressionResult VisitExpression_ElementAccessExpression(
      ElementAccessExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_DereferenceExpression(
      DereferenceExpression* expr) {
    UNIMPLEMENTED();
  }

  ExpressionResult VisitExpression_AssignmentExpression(
      AssignmentExpression* expr) {
    LocationResult location = VisitLocation(expr->location);
    if (expr->op) {
      UNIMPLEMENTED();
    } else {
      if (location.IsVariableAccess()) {
        out() << location.variable->name() << " = ";
        VisitExpression(expr->value);
      } else {
        UNIMPLEMENTED();
      }
    }
    return ExpressionResult(location.variable->type);
  }

  ExpressionResult VisitExpression_IncrementDecrementExpression(
      IncrementDecrementExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_NewExpression(NewExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_AssumeTypeImpossibleExpression(
      AssumeTypeImpossibleExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_StatementExpression(
      StatementExpression* expr) {
    UNIMPLEMENTED();
  }
  ExpressionResult VisitExpression_TryLabelExpression(
      TryLabelExpression* expr) {
    UNIMPLEMENTED();
  }

  struct LocationResult {
    enum Kind {
      kVariableAccess,
    };

    Kind kind;
    Binding<TSAValue>* variable;

    static LocationResult VariableAccess(Binding<TSAValue>* variable) {
      return LocationResult{
          .kind = Kind::kVariableAccess,
          .variable = variable,
      };
    }

    bool IsVariableAccess() const { return kind == Kind::kVariableAccess; }
  };

  LocationResult VisitLocation(Expression* expr) {
    switch (expr->kind) {
      case AstNode::Kind::kIdentifierExpression:
        return VisitLocation_IdentifierExpression(
            IdentifierExpression::cast(expr));
      default:
        UNIMPLEMENTED();
    }
  }

  LocationResult VisitLocation_IdentifierExpression(
      IdentifierExpression* expr) {
    if (expr->namespace_qualification.empty()) {
      if (std::optional<Binding<TSAValue>*> value =
              TryLookupLocalValue(expr->name->value)) {
        if (!expr->generic_arguments.empty()) {
          ReportError("cannot have generic parameters on local name ",
                      expr->name);
        }
        return LocationResult::VariableAccess(*value);
        // return (*value)->GetLocationReference(*value);
      }
    }

    UNIMPLEMENTED();
#if 0
    if (expr->IsThis()) {
      ReportError("\"this\" cannot be qualified");
    }
    QualifiedName name =
        QualifiedName(expr->namespace_qualification, expr->name->value);
    if (std::optional<Builtin*> builtin
          = Declarations::TryLookupBuiltin(name)) {
      if (GlobalContext::collect_language_server_data()) {
        LanguageServerData::AddDefinition(expr->name->pos,
                                          (*builtin)->Position());
      }
      // TODO(v8:12261): Consider collecting KytheData here.
      return LocationReference::Temporary(GetBuiltinCode(*builtin),
                                          "builtin " + expr->name->value);
    }
    if (!expr->generic_arguments.empty()) {
      GenericCallable* generic = Declarations::LookupUniqueGeneric(name);
      Callable* specialization =
          GetOrCreateSpecialization(SpecializationKey<GenericCallable>{
              generic, TypeVisitor::ComputeTypeVector(
                expr->generic_arguments)});
      if (Builtin* builtin = Builtin::DynamicCast(specialization)) {
        DCHECK(!builtin->IsExternal());
        return LocationReference::Temporary(GetBuiltinCode(builtin),
                                            "builtin " + expr->name->value);
      } else {
        ReportError("cannot create function pointer for non-builtin ",
                    generic->name());
      }
    }
    Value* value = Declarations::LookupValue(name);
    CHECK(value->Position().source.IsValid());
    if (auto stream = CurrentFileStreams::Get()) {
      stream->required_builtin_includes.insert(value->Position().source);
    }
    if (GlobalContext::collect_language_server_data()) {
      LanguageServerData::AddDefinition(expr->name->pos, value->name()->pos);
    }
    if (auto* constant = NamespaceConstant::DynamicCast(value)) {
      if (GlobalContext::collect_kythe_data()) {
        KytheData::AddConstantUse(expr->name->pos, constant);
      }
      if (constant->type()->IsConstexpr()) {
        return LocationReference::Temporary(
            VisitResult(constant->type(),
              constant->external_name() + "(state_)"),
            "namespace constant " + expr->name->value);
      }
      assembler().Emit(NamespaceConstantInstruction{constant});
      StackRange stack_range =
          assembler().TopRange(LoweredSlotCount(constant->type()));
      return LocationReference::Temporary(
          VisitResult(constant->type(), stack_range),
          "namespace constant " + expr->name->value);
    }
    ExternConstant* constant = ExternConstant::cast(value);
    if (GlobalContext::collect_kythe_data()) {
      KytheData::AddConstantUse(expr->name->pos, constant);
    }
    return LocationReference::Temporary(constant->value(),
                                        "extern value " + expr->name->value);
#endif
  }

  void BeginOutputToHFile(SourceId source, int indentation_levels) {
    DCHECK_NULL(current_source_data_);
    current_source_data_ = &GetPerSourceData(source);
    current_out_stream_ = &current_source_data_->h_stream;
    current_output_file_ = OutputFile::kH;
    indentation_levels_ = indentation_levels;
  }

  void BeginOutputToCCFile(SourceId source, int indentation_levels) {
    DCHECK_NULL(current_source_data_);
    current_source_data_ = &GetPerSourceData(source);
    current_out_stream_ = &current_source_data_->cc_stream;
    current_output_file_ = OutputFile::kCC;
    indentation_levels_ = indentation_levels;
  }

  void EndOutputToFile() {
    DCHECK_NOT_NULL(current_source_data_);
    current_source_data_ = nullptr;
    current_out_stream_ = nullptr;
    current_output_file_ = OutputFile::kNone;
  }

  std::stringstream* BeginOutputToBuffer(std::stringstream* stream) {
    std::swap(current_out_stream_, stream);
    return stream;
  }

  void EndOutputToBuffer(std::stringstream* stream,
                         std::stringstream* previous) {
    DCHECK_EQ(current_out_stream_, stream);
    current_out_stream_ = previous;
  }

  void WriteFile(const std::string& file_path, const std::string& content) {
    ReplaceFileContentsIfDifferent(file_path, content);
  }

  PerSourceData& GetPerSourceData(SourceId source) {
    auto it = per_source_data_.find(source);
    DCHECK_NE(it, per_source_data_.end());
    return it->second;
  }

  std::stringstream& out() {
    DCHECK_NOT_NULL(current_source_data_);
    DCHECK_NOT_NULL(current_out_stream_);
    return *current_out_stream_;
  }

  std::stringstream& out_i() {
    out() << indentation();
    return out();
  }

  std::string indentation(int diff = 0) {
    diff += indentation_levels_;
    return std::string(diff * kSpacesPerIndentationLevel, ' ');
  }

  template <typename T>
  T* target() {
    if constexpr (std::is_same_v<T, TargetBase>) {
      if (std::holds_alternative<MacroTarget>(target_)) {
        return &std::get<MacroTarget>(target_);
      }
      if (std::holds_alternative<BuiltinTarget>(target_)) {
        return &std::get<BuiltinTarget>(target_);
      }
    } else if (std::holds_alternative<T>(target_)) {
      return &std::get<T>(target_);
    }
    return nullptr;
  }

  void IncreaseIndentation(int levels = 1) { indentation_levels_ += levels; }

  void DecreaseIndentation(int levels = 1) {
    DCHECK_LE(levels, indentation_levels_);
    indentation_levels_ -= levels;
  }

  void EnterBlockBindingsScope(BlockBindings<TSAValue>* bindings) {
    block_bindings_.push_back(bindings);
  }

  void LeaveBlockBindingsScope(BlockBindings<TSAValue>* bindings) {
    DCHECK(!block_bindings_.empty());
    DCHECK_EQ(block_bindings_.back(), bindings);
    block_bindings_.pop_back();
  }

  BlockBindings<TSAValue>* current_block_bindings() {
    DCHECK(!block_bindings_.empty());
    return block_bindings_.back();
  }

  enum class SpecialAssemblerFunctionPrefix {
    kNoPrefix,
    kTemplatePrefix,
  };

  static constexpr SpecialAssemblerFunctionPrefix kTemplate =
      SpecialAssemblerFunctionPrefix::kTemplatePrefix;

  const char* __(SpecialAssemblerFunctionPrefix prefix =
                     SpecialAssemblerFunctionPrefix::kNoPrefix) {
    switch (current_output_file_) {
      case OutputFile::kNone:
        UNREACHABLE();
      case OutputFile::kH:
        if (prefix == SpecialAssemblerFunctionPrefix::kTemplatePrefix) {
          return "__ template ";
        }
        return "__ ";
      case OutputFile::kCC:
        return "";
    }
  }

  std::map<SourceId, PerSourceData> per_source_data_;
  PerSourceData* current_source_data_ = nullptr;
  std::stringstream* current_out_stream_ = nullptr;
  OutputFile current_output_file_ = OutputFile::kNone;
  std::variant<std::nullptr_t, MacroTarget, BuiltinTarget> target_;
  int indentation_levels_ = 0;
  std::vector<BlockBindings<TSAValue>*> block_bindings_;
};

void GenerateTSA(Ast& ast, const std::string& output_directory) {
  TSAGenerator generator;
  generator.Run(ast, output_directory);
}

}  // namespace v8::internal::torque
