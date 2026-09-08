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

struct TSALabel {
  const TypeVector parameter_types;
};

template <>
inline std::string Binding<TSALabel>::BindingTypeString() const {
  return "TSALabel";
}
template <>
inline bool Binding<TSALabel>::CheckWritten() const {
  return false;
}

class TSAGenerator : public AstVisitor<TSAGenerator> {
  static constexpr size_t kSpacesPerIndentationLevel = 2;

  DECLARE_CONTEXTUAL_VARIABLE(ValueBindingsManager, BindingsManager<TSAValue>);
  DECLARE_CONTEXTUAL_VARIABLE(LabelBindingsManager, BindingsManager<TSALabel>);

  struct TargetBase {
    const Type* return_type = nullptr;
    std::string return_label_name;
  };

  struct MacroTarget : public TargetBase {
    // We consider a macro tail-returnining if it has a single, unconditional
    // return statement at the very end of the body, such that we don't need
    // to GOTO to a return label, but can just simply return the ssa value.
    bool tail_returning;
  };

  struct BuiltinTarget : public TargetBase {};

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

        out() << "// TODO(tq2tsa): Remove this exception once the file is "
                 "manually reviewed.\nNO_SHADOW\n\n";

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

        out() << "#include "
                 "\"src/compiler/turboshaft/define-assembler-macros.inc\"\n\n";

        out() << "// TODO(tq2tsa): Remove this exception once the file is "
                 "manually reviewed.\nNO_SHADOW\n\n";

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

        out() << "// TODO(tq2tsa): Remove this exception once the file is "
                 "manually reviewed.\nRE_SHADOW\n\n";

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

        out() << "// TODO(tq2tsa): Remove this exception once the file is "
                 "manually reviewed.\nRE_SHADOW\n\n";

        out() << "#include "
                 "\"src/compiler/turboshaft/undef-assembler-macros.inc\"\n\n";

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

  bool IsTailReturning(TorqueMacro* macro) {
    auto body = BlockStatement::DynamicCast(macro->body().value());
    DCHECK_NOT_NULL(body);
    if (body->statements.empty()) {
      DCHECK(macro->signature().return_type->IsVoidOrNever());
      return true;
    }
    if (body->statements.size() > 1) return false;
    if (ReturnStatement::DynamicCast(body->statements[0])) {
      // We might have to restrict this more, e.g. return b ? x : y; could
      // require a return label.
      return true;
    }
    return false;
  }

  void GenerateMacro(TorqueMacro* macro, SourceId source) {
    if (!macro->SupportsTSA()) return;

    DCHECK(std::holds_alternative<std::nullptr_t>(target_));

    const Signature& signature = macro->signature();
    const Type* return_type = signature.return_type;

    BeginOutputToHFile(source, 1);
    const bool tail_returning = IsTailReturning(macro);
    const char* return_label_name = tail_returning ? "" : "_return";
    target_ = MacroTarget{TargetBase{.return_type = return_type,
                                     .return_label_name = return_label_name},
                          tail_returning};

    CurrentSourcePosition::Scope scource_position(macro->Position());
    ValueBindingsManager::Scope value_binding_scope;
    LabelBindingsManager::Scope label_binding_scope;

    if (return_type->IsVoidOrNever()) {
      out_i() << "void ";
    } else {
      out_i() << "V<" << return_type->GetGeneratedTNodeTypeName() << "> ";
    }
    out() << macro->ReadableName() << "(";

    BlockBindings<TSAValue> value_bindings(this, &ValueBindingsManager::Get());
    BlockBindings<TSALabel> label_bindings(this, &LabelBindingsManager::Get());
    constexpr bool kMarkAsUsed = true;

    size_t param_index = 0;
    // TODO(nicohartmann): Handle methods and `this` argument.
    for (; param_index < signature.parameter_types.types.size();
         ++param_index) {
      if (param_index != 0) out() << ", ";
      // TODO(nicohartmann): Could make some ConstOrV<> maybe.
      const Identifier* name = signature.parameter_names[param_index];
      const Type* type = signature.parameter_types.types[param_index];
      if (type->IsConstexpr()) {
        out() << type->TagglifiedCppTypeName() << " " << name->value;
      } else {
        out() << "V<" << type->GetGeneratedTNodeTypeName() << "> "
              << name->value;
      }
      value_bindings.Add(name, TSAValue{.type = type}, kMarkAsUsed);
    }

    // Now handle labels.
    for (const LabelDeclaration& label_info : signature.labels) {
      if (param_index != 0) out() << ", ";
      const std::string label_name = label_info.name->value;
      out() << "Label<";
      for (size_t type_index = 0; type_index < label_info.types.size();
           ++type_index) {
        if (type_index != 0) out() << ", ";
        out() << GetTypeName(label_info.types[type_index]);
      }
      out() << ">& " << label_name;
      label_bindings.Add(label_name,
                         TSALabel{.parameter_types = label_info.types},
                         kMarkAsUsed);
      ++param_index;
    }

    out() << ") {\n";

    // Generate the body.
    {
      IndentationScope indent(this);

      if (!tail_returning) {
        // If we have a return value, we need to set up the return label.
        out_i() << "Label<";
        if (!return_type->IsVoidOrNever()) {
          out() << return_type->GetGeneratedTNodeTypeName();
        }
        out() << "> " << return_label_name << "(this);\n\n";
      }

      auto macro_body = BlockStatement::DynamicCast(macro->body().value());
      DCHECK_NOT_NULL(macro_body);
      VisitBlockStatement(macro_body, false);

      if (!tail_returning) {
        out() << "\n";
        // Bind the return label and return the value (if any).
        if (return_type->IsVoidOrNever()) {
          out_i() << "BIND(" << return_label_name << ");\n";
        } else {
          out_i() << "BIND(" << return_label_name << ", return_value);\n";
          out_i() << "return return_value;\n";
        }
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
    ValueBindingsManager::Scope value_binding_scope;
    LabelBindingsManager::Scope label_binding_scope;

    out_i() << "TS_BUILTIN(" << builtin->ExternalName() << ", " << reducer_name
            << "AssemblerTS) {\n";

    {
      IndentationScope indent(this);
      BlockBindings<TSAValue> value_bindings(this,
                                             &ValueBindingsManager::Get());
      BlockBindings<TSALabel> label_bindings(this,
                                             &LabelBindingsManager::Get());

      size_t param_index = 0;
      for (; param_index < signature.parameter_types.types.size();
           ++param_index) {
        const Identifier* name = signature.parameter_names[param_index];
        const Type* type = signature.parameter_types.types[param_index];
        out_i() << "auto " << name->value << " = Parameter<"
                << type->GetGeneratedTNodeTypeName() << ">(Descriptor::k"
                << CamelifyString(name->value) << ");\n";
        constexpr bool kMarkAsUsed = true;
        value_bindings.Add(name, TSAValue{.type = type}, kMarkAsUsed);
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
      const std::vector<Binding<TSALabel>*>& labels,
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

  Callable* GetOrCreateSpecialization(
      const SpecializationKey<GenericCallable>& key) {
    if (std::optional<Callable*> specialization =
            key.generic->GetSpecialization(key.specialized_types)) {
      return *specialization;
    }
    return DeclarationVisitor::SpecializeImplicit(key);
  }

  // Try to lookup a callable with the provided argument types. Do not report
  // an error if no matching callable was found, but return false instead.
  // This is used to test the presence of overloaded field accessors.
  Callable* TryLookupCallable(const QualifiedName& name,
                              const TypeVector& parameter_types) {
    return LookupCallable(name, Declarations::TryLookup(name), parameter_types,
                          {}, {}, true);
  }

  template <class Container>
  Callable* LookupCallable(const QualifiedName& name,
                           const Container& declaration_container,
                           const TypeVector& parameter_types,
                           const std::vector<Binding<TSALabel>*>& labels,
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
      TypeArgumentInference inference = InferSpecializationTypes(
          generic, specialization_types, parameter_types);
      result = GetOrCreateSpecialization(
          SpecializationKey<GenericCallable>{generic, inference.GetResult()});
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
    // For some we skip the trailing semicolon.
    switch (node->expression->kind) {
      case AstNode::Kind::kTryLabelExpression:
        VisitExpression(node->expression);
        out() << "\n";
        break;
      default:
        out_i();
        VisitExpression(node->expression);
        out() << ";\n";
        break;
    }
    return node;
  }

  AstNode* VisitIfStatement(IfStatement* node) {
    if (node->is_constexpr) {
      UNIMPLEMENTED();
    }
    out_i() << "IF (";
    VisitExpression(node->condition);
    out() << ") {\n";

    // Then branch.
    {
      IndentationScope indent(this);
      if (BlockStatement* block = BlockStatement::DynamicCast(node->if_true)) {
        VisitBlockStatement(block, false);
      } else {
        Visit(node->if_true);
      }
    }
    out_i() << "}";

    // Else branch (if any).
    if (node->if_false.has_value()) {
      // TODO(nicohartmann): Handle proper bracing if not a BlockStatement.
      out() << " ELSE ";
      Visit(node->if_false.value());
    } else {
      out() << "\n";
    }

    return node;
  }

  AstNode* VisitWhileStatement(WhileStatement* node) {
    out_i() << "WHILE(";
    VisitExpression(node->condition);
    out() << ") ";
    Visit(node->body);
    return node;
  }

  AstNode* VisitTypeswitchStatement(TypeswitchStatement* node) {
    out_i() << "TYPESWITCH(";
    VisitExpression(node->expr);
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
      if (macro->tail_returning) {
        // Handle simple cases where return label can be avoided.
        if (macro->return_type->IsVoidOrNever()) {
          // We just fall to the end.
        } else {
          out_i() << "return ";
          VisitExpression(node->value.value());
          out() << ";\n";
        }
      } else {
        // Otherwise, we need to jump to the exit label.
        out_i() << "GOTO(" << macro->return_label_name;
        if (!macro->return_type->IsVoidOrNever()) {
          out() << ", ";
          VisitExpression(node->value.value());
        }
        out() << ");\n";
      }
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
    switch (node->kind) {
      case AssertStatement::AssertKind::kStaticAssert:
        UNIMPLEMENTED();
      case AssertStatement::AssertKind::kCheck:
        UNIMPLEMENTED();
      case AssertStatement::AssertKind::kDcheck: {
        out_i() << "TSA_DCHECK(this, ";
        VisitExpression(node->expression);
        out() << ");\n";
        break;
      }
      case AssertStatement::AssertKind::kSbxCheck:
        UNIMPLEMENTED();
    }
    return node;
  }
  AstNode* VisitTailCallStatement(TailCallStatement* node) {
    // TODO(nicohartmann): Implement.
    return node;
  }

  std::string GetTypeName(const Type* type) {
    std::string name = type->GetGeneratedTNodeTypeName();
    // We need to patch a few names for TSA.
    if (name == "UintPtrT") return "WordPtr";
    if (name == "Uint16T") return "Word32";
    if (name == "intptr") return "WordPtr";
    return name;
  }

  AstNode* VisitVarDeclarationStatement(VarDeclarationStatement* node) {
    if (node->const_qualified) {
      // For const we avoid actual variables and just define the ssa value.
      const Type* type = nullptr;
      if (node->type.has_value()) {
        type = TypeVisitor::ComputeType(node->type.value());
      }
      DCHECK_NOT_NULL(type);

      out_i() << "V<" << GetTypeName(type) << "> " << node->name->value;
      if (node->initializer.has_value()) {
        out() << " = ";
        VisitExpression(node->initializer.value());
        out() << ";\n";
        current_block_bindings_values()->Add(node->name, {.type = type});
      }
    } else {
      const Type* type = nullptr;
      if (node->type.has_value()) {
        type = TypeVisitor::ComputeType(node->type.value());
      } else {
        // Should use computed type of initializer.
        UNIMPLEMENTED();
      }
      DCHECK_NOT_NULL(type);

      out_i() << "ScopedVar<" << GetTypeName(type) << "> " << node->name->value
              << "(this";
      if (node->initializer.has_value()) {
        out() << ", ";
        VisitExpression(node->initializer.value());
      }
      out() << ");\n";
      current_block_bindings_values()->Add(node->name, {.type = type});
    }
    return node;
  }

  AstNode* VisitGotoStatement(GotoStatement* node) {
    Binding<TSALabel>* label = LookupLabel(node->label->value);
    USE(label);
    DCHECK_EQ(node->arguments.size(), label->parameter_types.size());

    out_i() << "GOTO(" << node->label->value;
    for (Expression* e : node->arguments) {
      out() << ", ";
      VisitExpression(e);
    }
    out() << ");\n";
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

  ExpressionResult GenerateCall(const Callable* callable,
                                const TypeVector& argument_types,
                                std::vector<std::string> argument_strings,
                                const std::vector<std::string>& label_names,
                                const TypeVector& specialization_types) {
    DCHECK_EQ(argument_types.size(), argument_strings.size());

    if (callable->IsExternMacro()) {
      // Handle a few special cases that we want to bypass.
      if (callable->ExternalName() == "Unsigned") {
        DCHECK_EQ(argument_strings.size(), 1);
        out() << argument_strings[0];
        return ExpressionResult{argument_types[0]};
      }
    }

    if (const RuntimeFunction* runtime_function =
            RuntimeFunction::DynamicCast(callable)) {
      DCHECK(label_names.empty());
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
      const auto prefix = specialization_types.empty()
                              ? SpecialAssemblerFunctionPrefix::kNoPrefix
                              : SpecialAssemblerFunctionPrefix::kTemplatePrefix;
      out() << __(prefix) << callable->ReadableName() << "(";
      bool first = true;
      for (size_t i = 0; i < callable->signature().implicit_count; ++i) {
        std::string implicit_name =
            callable->signature().parameter_names[i]->value;
        if (!first) out() << ", ";
        out() << implicit_name;
        first = false;
      }
      for (size_t i = 0; i < argument_strings.size(); ++i) {
        if (!first) out() << ", ";
        out() << argument_strings[i];
        first = false;
      }
      for (size_t i = 0; i < label_names.size(); ++i) {
        if (!first) out() << ", ";
        out() << label_names[i];
        first = false;
      }
      out() << ")";
    }
    return ExpressionResult(callable->signature().return_type);
  }

  std::vector<Binding<TSALabel>*> LabelsFromIdentifiers(
      const std::vector<Identifier*>& names) {
    std::vector<Binding<TSALabel>*> result;
    result.reserve(names.size());
    for (const auto& name : names) {
      Binding<TSALabel>* label = LookupLabel(name->value);
      result.push_back(label);
    }
    return result;
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

    TypeVector argument_types;
    std::vector<std::string> argument_strings;
    for (Expression* arg : expr->arguments) {
      OutputBufferScope output_buffer(this);
      ExpressionResult arg_result = VisitExpression(arg);
      DCHECK_NOT_NULL(arg_result.type);
      argument_types.push_back(arg_result.type);
      argument_strings.push_back(output_buffer.ToString());
    }

    std::vector<Binding<TSALabel>*> labels =
        LabelsFromIdentifiers(expr->labels);
    std::vector<std::string> label_names;
    for (const Identifier* ident : expr->labels) {
      label_names.push_back(ident->value);
    }
    DCHECK_EQ(labels.size(), label_names.size());

    Callable* callable =
        LookupCallable(name, Declarations::Lookup(name), argument_types, labels,
                       specialization_types);

    return GenerateCall(callable, argument_types, argument_strings, label_names,
                        specialization_types);
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

  std::optional<Binding<TSALabel>*> TryLookupLabel(const std::string& name) {
    return LabelBindingsManager::Get().TryLookup(name);
  }

  Binding<TSALabel>* LookupLabel(const std::string& name) {
    std::optional<Binding<TSALabel>*> label = TryLookupLabel(name);
    if (!label) ReportError("cannot find label ", name);
    return *label;
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
      // TODO(nicohartmann): Maybe find a better way to use namespace constants.
      // For now, we just emit the definition with which this was defined.
      VisitExpression(constant->body());
      return ExpressionResult(constant->type());
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
    std::string value = expr->literal;
    DCHECK_GE(value.length(), 2);
    DCHECK(value.starts_with("'"));
    DCHECK(value.ends_with("'"));
    out() << "\"" << value.substr(1, value.length() - 2) << "\"";
    return ExpressionResult(TypeOracle::GetConstexprStringType());
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
    std::optional<OutputBufferScope> output_buffer(this);
    ExpressionResult object = VisitExpression(expr->object);
    std::string object_string = output_buffer->ToString();
    output_buffer.reset();

    if (std::optional<const ClassType*> class_type =
            object.type->ClassSupertype()) {
      // This is a hack to distinguish the situation where we want to use
      // overloaded field accessors from when we want to create a reference.
      const Callable* explicit_overload = TryLookupCallable(
          QualifiedName{"." + expr->field->value}, {object.type});
      if (explicit_overload) {
        return GenerateCall(explicit_overload, {object.type}, {object_string},
                            {}, {});
      }
    }

    // This is a load. Store will be handled in VisitLocation.
    out() << __() << "LoadField(" << object_string << ", FIELD("
          << object.type->GetGeneratedTNodeTypeName() << ", "
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
    Visit(expr->statement);
    return ExpressionResult(TypeOracle::GetVoidType());
  }
  ExpressionResult VisitExpression_TryLabelExpression(
      TryLabelExpression* expr) {
    const std::string label_name = expr->label_block->label->value;
    TypeVector parameter_types;

    out() << "\n";
    out_i() << "// TODO(tq2tsa): This most likely needs some manual cleanup.\n";
    out_i() << "{\n";
    {
      IndentationScope indent(this);
      {
        BlockBindings<TSALabel> label_scope(this, &LabelBindingsManager::Get());
        // We first define all labels and add them to a new scope.
        out_i() << "Label<> done(this);\n";
        out_i() << "Label<";
        for (size_t i = 0; i < expr->label_block->parameters.names.size();
             ++i) {
          if (i != 0) out() << ", ";
          const Type* type =
              TypeVisitor::ComputeType(expr->label_block->parameters.types[i]);
          parameter_types.push_back(type);
          out() << GetTypeName(type);
        }
        out() << "> " << label_name << "(this";
        if (IsDeferred(expr->label_block->body)) {
          out() << "LabelBase::Likelyness::kUnlikely";
        }
        out() << ");\n";
        label_scope.Add(label_name,
                        TSALabel{.parameter_types = parameter_types});

        // Emit the try expression.
        const Type* result_type = TypeOracle::GetVoidType();
        if (StatementExpression* stmt =
                StatementExpression::DynamicCast(expr->try_expression)) {
          if (BlockStatement* block =
                  BlockStatement::DynamicCast(stmt->statement)) {
            VisitBlockStatement(block, false);
          } else {
            Visit(stmt->statement);
          }
        } else {
          ExpressionResult try_result = VisitExpression(expr->try_expression);
          result_type = try_result.type;
        }

        if (!result_type->IsNever()) {
          // We might return here, so we need to jump to a continuation after
          // the labels.
          out_i() << "GOTO(done);\n";
        }
      }

      out_i() << "BIND(" << label_name;
      for (size_t i = 0; i < parameter_types.size(); ++i) {
        const std::string name = expr->label_block->parameters.names[i]->value;
        out() << ", " << name;
        // TODO(nicohartmann): We just throw them into the current bindings, but
        // maybe we should introduce a separate block here.
        current_block_bindings_values()->Add(
            name, TSAValue{.type = parameter_types[i]});
      }
      out() << ");\n";
      // Emit body.
      if (BlockStatement* block =
              BlockStatement::DynamicCast(expr->label_block->body)) {
        VisitBlockStatement(block, false);
      } else {
        out_i();
        Visit(expr->label_block->body);
      }

      // Unconditional jump to the end.
      out_i() << "GOTO(done);\n";
      out_i() << "BIND(done);\n";
    }

    out_i() << "}\n";

    // TODO(nicohartmann): Handle actual return values.
    return ExpressionResult(TypeOracle::GetVoidType());
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
    block_bindings_values_.push_back(bindings);
  }

  void EnterBlockBindingsScope(BlockBindings<TSALabel>* bindings) {
    block_bindings_labels_.push_back(bindings);
  }

  void LeaveBlockBindingsScope(BlockBindings<TSAValue>* bindings) {
    DCHECK(!block_bindings_values_.empty());
    DCHECK_EQ(block_bindings_values_.back(), bindings);
    block_bindings_values_.pop_back();
  }

  void LeaveBlockBindingsScope(BlockBindings<TSALabel>* bindings) {
    DCHECK(!block_bindings_labels_.empty());
    DCHECK_EQ(block_bindings_labels_.back(), bindings);
    block_bindings_labels_.pop_back();
  }

  BlockBindings<TSAValue>* current_block_bindings_values() {
    DCHECK(!block_bindings_values_.empty());
    return block_bindings_values_.back();
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
  std::vector<BlockBindings<TSAValue>*> block_bindings_values_;
  std::vector<BlockBindings<TSALabel>*> block_bindings_labels_;
};

void GenerateTSA(Ast& ast, const std::string& output_directory) {
  TSAGenerator generator;
  generator.Run(ast, output_directory);
}

}  // namespace v8::internal::torque
