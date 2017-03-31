// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <functional>
#include <iostream>
#include <memory>

#include "src/asmjs/asm-typer.h"
#include "src/asmjs/asm-types.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/platform/platform.h"
#include "src/compiler.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;
namespace iw = v8::internal::wasm;

namespace v8 {
namespace internal {
namespace wasm {

namespace {
enum ValidationType {
  ValidateModule,
  ValidateGlobals,
  ValidateFunctionTables,
  ValidateExport,
  ValidateFunction,
  ValidateStatement,
  ValidateExpression,
};
}  // namespace

class AsmTyperHarnessBuilder {
 public:
  AsmTyperHarnessBuilder(const char* source, ValidationType type)
      : source_(source),
        validation_type_(type),
        handles_(),
        zone_(handles_.main_zone()),
        isolate_(CcTest::i_isolate()),
        ast_value_factory_(zone_, isolate_->ast_string_constants(),
                           isolate_->heap()->HashSeed()),
        factory_(isolate_->factory()),
        source_code_(
            factory_->NewStringFromUtf8(CStrVector(source)).ToHandleChecked()),
        script_(factory_->NewScript(source_code_)) {
    ParseInfo info(zone_, script_);
    info.set_allow_lazy_parsing(false);
    info.set_toplevel(true);
    info.set_ast_value_factory(&ast_value_factory_);
    info.set_ast_value_factory_owned(false);
    Parser parser(&info);

    if (!Compiler::ParseAndAnalyze(&info)) {
      std::cerr << "Failed to parse:\n" << source_ << "\n";
      CHECK(false);
    }

    outer_scope_ = info.script_scope();
    module_ = info.scope()
                  ->declarations()
                  ->AtForTest(0)
                  ->AsFunctionDeclaration()
                  ->fun();
    typer_.reset(new AsmTyper(isolate_, zone_, script_, module_));

    if (validation_type_ == ValidateStatement ||
        validation_type_ == ValidateExpression) {
      fun_scope_.reset(new AsmTyper::FunctionScope(typer_.get()));

      for (Declaration* decl : *module_->scope()->declarations()) {
        if (FunctionDeclaration* fun_decl = decl->AsFunctionDeclaration()) {
          fun_decl_ = fun_decl;
          break;
        }
      }
      CHECK_NOT_NULL(fun_decl_);
    }
  }

  struct VariableName {
    VariableName(const char* name, VariableMode mode)
        : name_(name), mode_(mode) {}
    VariableName(const VariableName&) = default;
    VariableName& operator=(const VariableName&) = default;

    const char* name_;
    const VariableMode mode_;
  };

  AsmTyperHarnessBuilder* WithLocal(VariableName var_name, AsmType* type) {
    CHECK(validation_type_ == ValidateStatement ||
          validation_type_ == ValidateExpression);
    auto* var = DeclareVariable(var_name);
    if (var->IsUnallocated()) {
      var->AllocateTo(VariableLocation::LOCAL, -1);
    }
    auto* var_info = new (zone_) AsmTyper::VariableInfo(type);
    var_info->set_mutability(AsmTyper::VariableInfo::kLocal);
    CHECK(typer_->AddLocal(var, var_info));
    return this;
  }

  AsmTyperHarnessBuilder* WithGlobal(VariableName var_name, AsmType* type) {
    auto* var = DeclareVariable(var_name);
    if (var->IsUnallocated()) {
      var->AllocateTo(VariableLocation::MODULE, -1);
    }
    if (type != nullptr) {
      auto* var_info = new (zone_) AsmTyper::VariableInfo(type);
      var_info->set_mutability(AsmTyper::VariableInfo::kMutableGlobal);
      CHECK(typer_->AddGlobal(var, var_info));
    }
    return this;
  }

  AsmTyperHarnessBuilder* WithGlobal(
      VariableName var_name, std::function<AsmType*(Zone*)> type_creator) {
    return WithGlobal(var_name, type_creator(zone_));
  }

  AsmTyperHarnessBuilder* WithUndefinedGlobal(
      VariableName var_name, std::function<AsmType*(Zone*)> type_creator) {
    auto* type = type_creator(zone_);
    CHECK(type->AsFunctionType() != nullptr ||
          type->AsFunctionTableType() != nullptr);
    WithGlobal(var_name, type);
    auto* var_info = typer_->Lookup(DeclareVariable(var_name));
    CHECK(var_info);
    MessageLocation location;
    var_info->SetFirstForwardUse(location);
    return this;
  }

  AsmTyperHarnessBuilder* WithImport(VariableName var_name,
                                     AsmTyper::StandardMember standard_member) {
    auto* var = DeclareVariable(var_name);
    if (var->IsUnallocated()) {
      var->AllocateTo(VariableLocation::LOCAL, -1);
    }
    AsmTyper::VariableInfo* var_info = nullptr;
    auto* stdlib_map = &typer_->stdlib_math_types_;
    switch (standard_member) {
      case AsmTyper::kHeap:
      case AsmTyper::kStdlib:
      case AsmTyper::kModule:
      case AsmTyper::kNone:
        CHECK(false);
      case AsmTyper::kFFI:
        stdlib_map = nullptr;
        var_info = new (zone_) AsmTyper::VariableInfo(AsmType::FFIType(zone_));
        var_info->set_mutability(AsmTyper::VariableInfo::kImmutableGlobal);
        break;
      case AsmTyper::kInfinity:
      case AsmTyper::kNaN:
        stdlib_map = &typer_->stdlib_types_;
      default:
        break;
    }

    if (var_info == nullptr) {
      for (auto iter : *stdlib_map) {
        if (iter.second->standard_member() == standard_member) {
          var_info = iter.second;
          break;
        }
      }

      CHECK(var_info != nullptr);
      var_info = var_info->Clone(zone_);
    }

    CHECK(typer_->AddGlobal(var, var_info));
    return this;
  }

  AsmTyperHarnessBuilder* WithReturnType(AsmType* type) {
    CHECK(type->IsReturnType());
    CHECK(typer_->return_type_ == AsmType::None());
    typer_->return_type_ = type;
    return this;
  }

  AsmTyperHarnessBuilder* WithStdlib(VariableName var_name) {
    auto* var = DeclareVariable(var_name);
    auto* var_info =
        AsmTyper::VariableInfo::ForSpecialSymbol(zone_, AsmTyper::kStdlib);
    CHECK(typer_->AddGlobal(var, var_info));
    return this;
  }

  AsmTyperHarnessBuilder* WithHeap(VariableName var_name) {
    auto* var = DeclareVariable(var_name);
    auto* var_info =
        AsmTyper::VariableInfo::ForSpecialSymbol(zone_, AsmTyper::kHeap);
    CHECK(typer_->AddGlobal(var, var_info));
    return this;
  }

  AsmTyperHarnessBuilder* WithFFI(VariableName var_name) {
    auto* var = DeclareVariable(var_name);
    auto* var_info =
        AsmTyper::VariableInfo::ForSpecialSymbol(zone_, AsmTyper::kFFI);
    CHECK(typer_->AddGlobal(var, var_info));
    return this;
  }

  bool Succeeds() {
    CHECK(validation_type_ == ValidateModule ||
          validation_type_ == ValidateGlobals ||
          validation_type_ == ValidateFunctionTables ||
          validation_type_ == ValidateExport ||
          validation_type_ == ValidateFunction ||
          validation_type_ == ValidateStatement);

    if (validation_type_ == ValidateStatement) {
      CHECK(typer_->return_type_ != AsmType::None());
      if (ValidateAllStatements(fun_decl_)) {
        return true;
      }
    } else if (typer_->Validate()) {
      return true;
    }

    std::unique_ptr<char[]> msg = i::MessageHandler::GetLocalizedMessage(
        isolate_, typer_->error_message());
    std::cerr << "Asm validation failed: " << msg.get() << "\n";
    return false;
  }

  bool SucceedsWithExactType(AsmType* type) {
    CHECK(validation_type_ == ValidateExpression);
    auto* validated_as = ValidateExpressionStatment(fun_decl_);
    if (validated_as == AsmType::None()) {
      std::unique_ptr<char[]> msg = i::MessageHandler::GetLocalizedMessage(
          isolate_, typer_->error_message());
      std::cerr << "Validation failure: " << msg.get() << "\n";
      return false;
    } else if (validated_as != type) {
      std::cerr << "Validation succeeded with wrong type "
                << validated_as->Name() << " (vs. " << type->Name() << ").\n";
      return false;
    }

    return true;
  }

  bool FailsWithMessage(const char* error_message) {
    CHECK(validation_type_ == ValidateModule ||
          validation_type_ == ValidateGlobals ||
          validation_type_ == ValidateFunctionTables ||
          validation_type_ == ValidateExport ||
          validation_type_ == ValidateFunction ||
          validation_type_ == ValidateStatement ||
          validation_type_ == ValidateExpression);

    bool success;
    if (validation_type_ == ValidateStatement) {
      CHECK(typer_->return_type_ != AsmType::None());
      success = ValidateAllStatements(fun_decl_);
    } else if (validation_type_ == ValidateExpression) {
      success = ValidateExpressionStatment(fun_decl_) != AsmType::None();
    } else {
      success = typer_->Validate();
    }

    if (success) {
      std::cerr << "Asm validation succeeded\n";
      return false;
    }

    std::unique_ptr<char[]> msg = i::MessageHandler::GetLocalizedMessage(
        isolate_, typer_->error_message());
    if (std::strstr(msg.get(), error_message) == nullptr) {
      std::cerr << "Asm validation failed with the wrong error message:\n"
                   "Expected to contain '"
                << error_message << "'\n"
                                    "       Actually is  '"
                << msg.get() << "'\n";
      return false;
    }

    return true;
  }

 private:
  Variable* DeclareVariable(VariableName var_name) {
    auto* name_ast_string = ast_value_factory_.GetOneByteString(var_name.name_);
    ast_value_factory_.Internalize(isolate_);
    return var_name.mode_ == DYNAMIC_GLOBAL
               ? outer_scope_->DeclareDynamicGlobal(name_ast_string,
                                                    NORMAL_VARIABLE)
               : module_->scope()->DeclareLocal(name_ast_string, VAR,
                                                kCreatedInitialized,
                                                NORMAL_VARIABLE);
  }

  bool ValidateAllStatements(FunctionDeclaration* fun_decl) {
    AsmTyper::FlattenedStatements iter(zone_, fun_decl->fun()->body());
    while (auto* curr = iter.Next()) {
      if (typer_->ValidateStatement(curr) == AsmType::None()) {
        return false;
      }
    }
    return true;
  }

  AsmType* ValidateExpressionStatment(FunctionDeclaration* fun_decl) {
    AsmTyper::FlattenedStatements iter(zone_, fun_decl->fun()->body());
    AsmType* ret = AsmType::None();
    bool last_was_expression_statement = false;
    while (auto* curr = iter.Next()) {
      if (auto* expr_stmt = curr->AsExpressionStatement()) {
        last_was_expression_statement = true;
        if ((ret = typer_->ValidateExpression(expr_stmt->expression())) ==
            AsmType::None()) {
          break;
        }
      } else {
        ret = AsmType::None();
        last_was_expression_statement = true;
        if (typer_->ValidateStatement(curr) == AsmType::None()) {
          break;
        }
      }
    }
    CHECK(last_was_expression_statement || ret == AsmType::None());
    return ret;
  }

  std::string source_;
  ValidationType validation_type_;
  HandleAndZoneScope handles_;
  Zone* zone_;
  Isolate* isolate_;
  AstValueFactory ast_value_factory_;
  Factory* factory_;
  Handle<String> source_code_;
  Handle<Script> script_;

  DeclarationScope* outer_scope_;
  FunctionLiteral* module_;
  FunctionDeclaration* fun_decl_;
  std::unique_ptr<AsmTyper> typer_;
  std::unique_ptr<AsmTyper::FunctionScope> fun_scope_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

namespace {

struct ValidationInput {
  ValidationInput(const std::string& source, iw::ValidationType type)
      : source_(source), type_(type) {}

  const std::string source_;
  const iw::ValidationType type_;
};

std::unique_ptr<iw::AsmTyperHarnessBuilder> ValidationOf(
    ValidationInput input) {
  return std::unique_ptr<iw::AsmTyperHarnessBuilder>(
      new iw::AsmTyperHarnessBuilder(input.source_.c_str(), input.type_));
}

ValidationInput Module(const char* source) {
  return ValidationInput(source, iw::ValidateModule);
}

std::string WrapInFunction(const char* source, bool needs_use_asm) {
  if (needs_use_asm) {
    return std::string(
               "function foo() {\n"
               "  'use asm';\n"
               "  ") +
           source +
           "\n"
           "}";
  }

  return std::string(
             "function bar() {\n"
             "  ") +
         source +
         "\n"
         "}\n"
         "return {b: bar};\n";
}

ValidationInput Globals(const char* source) {
  static const bool kNeedsUseAsm = true;
  return ValidationInput(WrapInFunction(source, kNeedsUseAsm),
                         iw::ValidateGlobals);
}

ValidationInput FunctionTables(const char* source) {
  static const bool kNeedsUseAsm = true;
  return ValidationInput(WrapInFunction(source, kNeedsUseAsm),
                         iw::ValidateFunctionTables);
}

ValidationInput Export(const char* source) {
  static const bool kNeedsUseAsm = true;
  return ValidationInput(WrapInFunction(source, kNeedsUseAsm),
                         iw::ValidateExport);
}

ValidationInput Function(const char* source) {
  static const bool kNeedsUseAsm = true;
  return ValidationInput(WrapInFunction(source, kNeedsUseAsm),
                         iw::ValidateFunction);
}

ValidationInput Statement(const char* source) {
  static const bool kDoesNotNeedUseAsm = false;
  static const bool kNeedsUseAsm = true;
  return ValidationInput(
      WrapInFunction(WrapInFunction(source, kDoesNotNeedUseAsm).c_str(),
                     kNeedsUseAsm),
      iw::ValidateStatement);
}

ValidationInput Expression(const char* source) {
  static const bool kDoesNotNeedUseAsm = false;
  static const bool kNeedsUseAsm = true;
  return ValidationInput(
      WrapInFunction(WrapInFunction(source, kDoesNotNeedUseAsm).c_str(),
                     kNeedsUseAsm),
      iw::ValidateExpression);
}

iw::AsmTyperHarnessBuilder::VariableName Var(const char* name) {
  return iw::AsmTyperHarnessBuilder::VariableName(name, VAR);
}

iw::AsmTyperHarnessBuilder::VariableName DynamicGlobal(const char* name) {
  return iw::AsmTyperHarnessBuilder::VariableName(name, DYNAMIC_GLOBAL);
}

TEST(MissingUseAsmDirective) {
  v8::V8::Initialize();

  // We can't test the empty input ("") because the AsmTyperHarnessBuilder will
  // CHECK if there's no function in the top-level scope.
  const char* kTests[] = {"function module(){}",
                          "function module(){ use_asm; }",
                          "function module(){ \"use asm \"; }",
                          "function module(){ \" use asm \"; }",
                          "function module(){ \"use Asm\"; }"};

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const char* module = kTests[ii];
    if (!ValidationOf(Module(module))
             ->FailsWithMessage("Missing \"use asm\"")) {
      std::cerr << "Test:\n" << module;
      CHECK(false);
    }
  }
}

TEST(InvalidModuleSignature) {
  v8::V8::Initialize();

  const struct {
    const char* module;
    const char* error_message;
  } kTests[] = {
      {"function eval(){ \"use asm\"; }",
       "Invalid asm.js identifier in module name"},
      {"function arguments(){ \"use asm\"; }",
       "Invalid asm.js identifier in module name"},
      {"function module(eval){ \"use asm\"; }",
       "Invalid asm.js identifier in module parameter"},
      {"function module(arguments){ \"use asm\"; }",
       "Invalid asm.js identifier in module parameter"},
      {"function module(stdlib, eval){ \"use asm\"; }",
       "Invalid asm.js identifier in module parameter"},
      {"function module(stdlib, arguments){ \"use asm\"; }",
       "Invalid asm.js identifier in module parameter"},
      {"function module(stdlib, foreign, eval){ \"use asm\"; }",
       "Invalid asm.js identifier in module parameter"},
      {"function module(stdlib, foreign, arguments){ \"use asm\"; }",
       "Invalid asm.js identifier in module parameter"},
      {"function module(stdlib, foreign, heap, eval){ \"use asm\"; }",
       "asm.js modules may not have more than three parameters"},
      {"function module(stdlib, foreign, heap, arguments){ \"use asm\"; }",
       "asm.js modules may not have more than three parameters"},
      {"function module(module){ \"use asm\"; }",
       "Redeclared identifier in module parameter"},
      {"function module(stdlib, module){ \"use asm\"; }",
       "Redeclared identifier in module parameter"},
      {"function module(stdlib, stdlib){ \"use asm\"; }",
       "Redeclared identifier in module parameter"},
      {"function module(stdlib, foreign, module){ \"use asm\"; }",
       "Redeclared identifier in module parameter"},
      {"function module(stdlib, foreign, stdlib){ \"use asm\"; }",
       "Redeclared identifier in module parameter"},
      {"function module(stdlib, foreign, foreign){ \"use asm\"; }",
       "Redeclared identifier in module parameter"},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Module(test->module))
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->module;
      CHECK(false);
    }
  }
}

TEST(ErrorsInGlobalVariableDefinition) {
  const struct {
    const char* decl;
    const char* error_message;
  } kTests[] = {
      {"var v;", "Global variable missing initializer"},
      {"var v = uninitialized;", "Undeclared identifier in global"},
      {"var v = 'use asm';", "type annotation - forbidden literal"},
      {"var v = 4294967296;", " - forbidden literal"},
      {"var v = not_fround;", "initialize a global must be a const"},
      {"var v = not_fround(1);", "expected call fround(literal)"},
      {"var v = __fround__(1.0);", "expected call fround(literal)"},
      {"var v = fround(1.0, 1.0);", "expected call fround(literal)"},
      {"var v = fround(not_fround);", "literal argument for call to fround"},
      {"var v = i?0:1;", "Invalid global variable initializer"},
      {"var v = stdlib.nan", "Invalid import"},
      {"var v = stdlib.Math.nan", "Invalid import"},
      {"var v = stdlib.Mathh.E", "Invalid import"},
      {"var v = stdlib.Math", "Invalid import"},
      {"var v = Stdlib.Math.E", "Invalid import"},
      {"var v = stdlib.Math.E[0]", "Invalid import"},
      {"var v = stdlibb.NaN", "Invalid import"},
      {"var v = ffi.NaN[0]", "Invalid import"},
      {"var v = heap.NaN[0]", "Invalid import"},
      {"var v = ffi.foo * 2.0;", "unrecognized annotation"},
      {"var v = ffi.foo|1;", "unrecognized annotation"},
      {"var v = ffi()|0;", "must import member"},
      {"var v = +ffi();", "must import member"},
      {"var v = ffi().a|0;", "object lookup failed"},
      {"var v = +ffi().a;", "object lookup failed"},
      {"var v = sstdlib.a|0;", "object lookup failed"},
      {"var v = +sstdlib.a;", "object lookup failed"},
      {"var v = stdlib.NaN|0;", "object is not the ffi"},
      {"var v = +stdlib.NaN;", "object is not the ffi"},
      {"var v = new f()", "Invalid type after new"},
      {"var v = new stdli.Uint8Array(heap)", "Unknown stdlib member in heap"},
      {"var v = new stdlib.dd(heap)", "Unknown stdlib member in heap"},
      {"var v = new stdlib.Math.fround(heap)", "Type is not a heap view type"},
      {"var v = new stdlib.Uint8Array(a, b)", "Invalid number of arguments"},
      {"var v = new stdlib.Uint8Array(heap())", "should be the module's heap"},
      {"var v = new stdlib.Uint8Array(heap_)", "instead of heap parameter"},
      {"var v = new stdlib.Uint8Array(ffi)", "should be the module's heap"},
      {"var eval = 0;", "in global variable"},
      {"var eval = 0.0;", "in global variable"},
      {"var eval = fround(0.0);", "in global variable"},
      {"var eval = +ffi.a;", "in global variable"},
      {"var eval = ffi.a|0;", "in global variable"},
      {"var eval = ffi.a;", "in global variable"},
      {"var eval = new stdlib.Uint8Array(heap);", "in global variable"},
      {"var arguments = 0;", "in global variable"},
      {"var arguments = 0.0;", "in global variable"},
      {"var arguments = fround(0.0);", "in global variable"},
      {"var arguments = +ffi.a;", "in global variable"},
      {"var arguments = ffi.a|0;", "in global variable"},
      {"var arguments = ffi.a;", "in global variable"},
      {"var arguments = new stdlib.Uint8Array(heap);", "in global variable"},
      {"var a = 0, a = 0.0;", "Redefined global variable"},
      {"var a = 0; var a = 0;", "Redefined global variable"},
      {"var a = 0, b = 0; var a = 0;", "Redefined global variable"},
      {"var a = 0, b = 0; var b = 0, a = 0.0;", "Redefined global variable"},
      {"var a = stdlib.Int8Array", "Heap view types can not be aliased"},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Globals(test->decl))
             ->WithStdlib(DynamicGlobal("stdlib"))
             ->WithFFI(DynamicGlobal("ffi"))
             ->WithHeap(DynamicGlobal("heap"))
             ->WithGlobal(DynamicGlobal("not_fround"), iw::AsmType::Int())
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->decl;
      CHECK(false);
    }
  }
}

TEST(ErrorsInFunctionTableDefinition) {
  const struct {
    const char* tables;
    const char* error_message;
  } kTests[] = {
      {"var a = [a, a, a];", "Invalid length for function pointer table"},
      {"var a = [d2s0()];", "must be a function name"},
      {"var a = [d2s44];", "Undefined identifier in function pointer"},
      {"var a = [fround];", "not be a member of the standard library"},
      {"var a = [imul];", "not be a member of the standard library"},
      {"var a = [ffi_import];", "must be an asm.js function"},
      {"var a = [dI];", "must be an asm.js function"},
      {"var a = [d2s0, d2s1, d2s0, f2s0];", "mismatch in function pointer"},
      {"var eval = [d2s0, d2s1];", "asm.js identifier in function table name"},
      {"var arguments = [d2s0, d2s1];", "asm.js identifier in function table"},
      {"var foo = [d2s0, d2s1];",
       "Identifier redefined as function pointer table"},
      {"var I = [d2s0, d2s1];",
       "Identifier redefined as function pointer table"},
      {"var d2s = [d2f0, d2f1];", "redefined as function pointer table"},
      {"var d2s_t = [d2s0];", "Function table size mismatch"},
      {"var d2s_t = [d2f0, d2f1];", "initializer does not match previous"},
  };

  auto d2s = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Signed());
    ret->AsFunctionType()->AddArgument(iw::AsmType::Double());
    return ret;
  };

  auto d2s_tbl = [](Zone* zone) -> iw::AsmType* {
    auto* d2s = iw::AsmType::Function(zone, iw::AsmType::Signed());
    d2s->AsFunctionType()->AddArgument(iw::AsmType::Double());

    auto* ret = iw::AsmType::FunctionTableType(zone, 2, d2s);
    return ret;
  };

  auto f2s = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Signed());
    ret->AsFunctionType()->AddArgument(iw::AsmType::Float());
    return ret;
  };

  auto d2f = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Float());
    ret->AsFunctionType()->AddArgument(iw::AsmType::Double());
    return ret;
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(FunctionTables(test->tables))
             ->WithImport(DynamicGlobal("ffi_import"), iw::AsmTyper::kFFI)
             ->WithImport(DynamicGlobal("imul"), iw::AsmTyper::kMathImul)
             ->WithImport(DynamicGlobal("E"), iw::AsmTyper::kMathE)
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithImport(DynamicGlobal("floor"), iw::AsmTyper::kMathFround)
             ->WithGlobal(DynamicGlobal("d2s0"), d2s)
             ->WithGlobal(DynamicGlobal("d2s1"), d2s)
             ->WithGlobal(DynamicGlobal("f2s0"), f2s)
             ->WithGlobal(DynamicGlobal("f2s1"), f2s)
             ->WithGlobal(DynamicGlobal("d2f0"), d2f)
             ->WithGlobal(DynamicGlobal("d2f1"), d2f)
             ->WithGlobal(DynamicGlobal("dI"), iw::AsmType::Int())
             ->WithGlobal(Var("I"), iw::AsmType::Int())
             ->WithUndefinedGlobal(Var("d2s"), d2s)
             ->WithUndefinedGlobal(Var("d2s_t"), d2s_tbl)
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->tables;
      CHECK(false);
    }
  }
}

TEST(ErrorsInModuleExport) {
  const struct {
    const char* module_export;
    const char* error_message;
  } kTests[] = {
      {"", "Missing asm.js module export"},
      {"return;", "Unrecognized expression in asm.js module export expression"},
      {"return f;", "Undefined identifier in asm.js module export"},
      {"return f();", "Unrecognized expression in asm.js module export"},
      {"return d2s_tbl;", "cannot export function tables"},
      {"return min;", "cannot export standard library functions"},
      {"return ffi;", "cannot export foreign functions"},
      {"return I;", "is not an asm.js function"},
      {"return {'a': d2s_tbl}", "cannot export function tables"},
      {"return {'a': min}", "cannot export standard library functions"},
      {"return {'a': ffi}", "cannot export foreign functions"},
      {"return {'a': f()}", "must be an asm.js function name"},
      {"return {'a': f}", "Undefined identifier in asm.js module export"},
      {"function v() { a(); } return {b: d2s}",
       "Invalid call of existing global function"},
      {"function v() {} return {b: v, 'a': d2s_tbl}",
       "cannot export function tables"},
      {"function v() {} return {b: v, 'a': min}",
       "cannot export standard library"},
      {"function v() {} return {b: v, 'a': ffi}",
       "cannot export foreign functions"},
      {"function v() {} return {b: v, 'a': f()}",
       "must be an asm.js function name"},
      {"function v() {} return {b: v, 'a': f}",
       "Undefined identifier in asm.js module"},
  };

  auto d2s_tbl = [](Zone* zone) -> iw::AsmType* {
    auto* d2s = iw::AsmType::Function(zone, iw::AsmType::Signed());
    d2s->AsFunctionType()->AddArgument(iw::AsmType::Double());

    auto* ret = iw::AsmType::FunctionTableType(zone, 2, d2s);
    return ret;
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Export(test->module_export))
             ->WithGlobal(DynamicGlobal("d2s_tbl"), d2s_tbl)
             ->WithImport(DynamicGlobal("min"), iw::AsmTyper::kMathMin)
             ->WithImport(DynamicGlobal("ffi"), iw::AsmTyper::kFFI)
             ->WithGlobal(DynamicGlobal("I"), iw::AsmType::Int())
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->module_export;
      CHECK(false);
    }
  }
}

TEST(ErrorsInFunction) {
  auto d2s = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Signed());
    ret->AsFunctionType()->AddArgument(iw::AsmType::Double());
    return ret;
  };

  const struct {
    const char* function;
    const char* error_message;
  } kTests[] = {
      {"function f(eval) {"
       "  eval = eval|0;"
       "}\n",
       "Invalid asm.js identifier in parameter name"},
      {"function f(arguments) {"
       "  arguments = arguments|0;"
       "}\n",
       "Invalid asm.js identifier in parameter name"},
      // The following error should actually be a "redeclared local," but the
      // AST "hides" the first parameter from us, so the parameter type checking
      // will fail because the validator will think that the a = a|0 is
      // annotating the second parameter.
      {"function f(a, a) {\n"
       "  a = a|0;\n"
       "  a = +a;\n"
       "}\n",
       "Incorrect parameter type annotations"},
      {"function f(b, a) {\n"
       "  if (0) return;\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "}\n",
       "Incorrect parameter type annotations"},
      {"function f(b, a) {\n"
       "  f();\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "}\n",
       "Incorrect parameter type annotations"},
      {"function f(b, a) {\n"
       "  f.a = 0;\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "}\n",
       "Incorrect parameter type annotations"},
      {"function f(b, a) {\n"
       "  a = a|0;\n"
       "  b = +b;\n"
       "}\n",
       "Incorrect parameter type annotations"},
      {"function f(b, a) {\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "  var eval = 0;\n"
       "}\n",
       "Invalid asm.js identifier in local variable"},
      {"function f(b, a) {\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "  var b = 0;\n"
       "}\n",
       "Redeclared local"},
      {"function f(b, a) {\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "  var c = 0, c = 1.0;\n"
       "}\n",
       "Redeclared local"},
      {"function f(b, a) {\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "  var c = 0; var c = 1.0;\n"
       "}\n",
       "Redeclared local"},
      {"function f(b, a) {\n"
       "  b = +b;\n"
       "  a = a|0;\n"
       "  f();\n"
       "  var c = 0;\n"
       "}\n",
       "Local variable missing initializer in asm.js module"},
      {"function f(a) {\n"
       "  a = a|0;\n"
       "  var x = a;\n"
       "}\n",
       "variable declaration initializer must be const"},
      {"function f() {\n"
       "  var x = 1+i;\n"
       "}\n",
       "should be a literal, const, or fround(literal"},
      {"function f() {\n"
       "  var x = a;\n"
       "}\n",
       "Undeclared identifier in variable declaration initializer"},
      {"function f() {\n"
       "  function ff() {}\n"
       "}\n",
       "Functions may only define inner variables"},
      {"function f() {\n"
       "  return a+1;\n"
       "}\n",
       "Invalid return type annotation"},
      {"function f() {\n"
       "  return ~~x;\n"
       "}\n",
       "Invalid return type annotation"},
      {"function f() {\n"
       "  return d();\n"
       "}\n",
       "Invalid function call in return statement"},
      {"function f() {\n"
       "  return 'use asm';\n"
       "}\n",
       "Invalid literal in return statement"},
      {"function f() {\n"
       " return 2147483648;\n"
       "}\n",
       "Invalid literal in return statement"},
      {"function f(a) {\n"
       "  a = a|0;\n"
       "  return a;\n"
       "}\n",
       "in return statement is not const"},
      {"function f() {\n"
       "  return a;\n"
       "}\n",
       "Undeclared identifier in return statement"},
      {"function f() {\n"
       "  return i?0:1;\n"
       "}\n",
       "Invalid return type expression"},
      {"function f() {\n"
       "  return stdlib.Math.E;"
       "}\n",
       "Invalid return type expression"},
      {"function f() {\n"
       "  return E[0];"
       "}\n",
       "Invalid return type expression"},
      {"function I() {}\n", "Identifier redefined as function"},
      {"function foo() {}\n", "Identifier redefined as function"},
      {"function d2s() {}\n", "Identifier redefined (function name)"},
      {"function d2s(x) {\n"
       "  x = x|0;\n"
       "  return -1;\n"
       "}\n",
       "Identifier redefined (function name)"},
      {"function d2s(x) {\n"
       "  x = +x;\n"
       "  return -1.0;\n"
       "}\n",
       "Identifier redefined (function name)"},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Function(test->function))
             ->WithGlobal(Var("I"), iw::AsmType::Int())
             ->WithGlobal(Var("d2s"), d2s)
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->function;
      CHECK(false);
    }
  }
}

TEST(ErrorsInStatement) {
  const struct {
    const char* statement;
    const char* error_message;
  } kTests[] = {
      {"if (fround(1));", "If condition must be type int"},
      {"return;", "Type mismatch in return statement"},
      {"return +1.0;", "Type mismatch in return statement"},
      {"return +d()", "Type mismatch in return statement"},
      {"while (fround(1));", "While condition must be type int"},
      {"do {} while (fround(1));", "Do {} While condition must be type int"},
      {"for (;fround(1););", "For condition must be type int"},
      {"switch(flocal){ case 0: return 0; }", "Switch tag must be signed"},
      {"switch(slocal){ default: case 0: return 0; }",
       "Switch default must appear last"},
      {"switch(slocal){ case 1: case 1: return 0; }", "Duplicated case label"},
      {"switch(slocal){ case 1: case 0: break; case 1: return 0; }",
       "Duplicated case label"},
      {"switch(slocal){ case 1.0: return 0; }",
       "Case label must be a 32-bit signed integer"},
      {"switch(slocal){ case 1.0: return 0; }",
       "Case label must be a 32-bit signed integer"},
      {"switch(slocal){ case -100000: case 2147483647: return 0; }",
       "Out-of-bounds case"},
      {"switch(slocal){ case 2147483648: return 0; }",
       "Case label must be a 32-bit signed"},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Statement(test->statement))
             ->WithReturnType(iw::AsmType::Signed())
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithLocal(DynamicGlobal("flocal"), iw::AsmType::Float())
             ->WithLocal(DynamicGlobal("slocal"), iw::AsmType::Signed())
             ->WithGlobal(DynamicGlobal("d"), nullptr)
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->statement;
      CHECK(false);
    }
  }
}

TEST(ErrorsInExpression) {
  auto d2d = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Double());
    ret->AsFunctionType()->AddArgument(iw::AsmType::Double());
    return ret;
  };

  auto d2s_tbl = [](Zone* zone) -> iw::AsmType* {
    auto* d2s = iw::AsmType::Function(zone, iw::AsmType::Signed());
    d2s->AsFunctionType()->AddArgument(iw::AsmType::Double());

    auto* ret = iw::AsmType::FunctionTableType(zone, 2, d2s);
    return ret;
  };

  const struct {
    const char* expression;
    const char* error_message;
  } kTests[] = {
      {"noy_a_function();", "Unanotated call to a function must be a call to"},
      {"a = 0;", "Undeclared identifier"},
      // we can't verify the module's name being referenced here because
      // expression validation does not invoke ValidateModule, which sets up the
      // module information in the AsmTyper.
      {"stdlib", "accessed by ordinary expressions"},
      {"ffi", "accessed by ordinary expressions"},
      {"heap", "accessed by ordinary expressions"},
      {"d2d", "accessed by ordinary expression"},
      {"fround", "accessed by ordinary expression"},
      {"d2s_tbl", "accessed by ordinary expression"},
      {"ilocal = +1.0", "Type mismatch in assignment"},
      {"!dlocal", "Invalid type for !"},
      {"2 * dlocal", "Invalid types for intish *"},
      {"dlocal * 2", "Invalid types for intish *"},
      {"1048577 * ilocal", "Invalid operands for *"},
      {"1048577 / ilocal", "Invalid operands for /"},
      {"1048577 % dlocal", "Invalid operands for %"},
      {"1048577 * dlocal", "Invalid operands for *"},
      {"1048577 / dlocal", "Invalid operands for /"},
      {"1048577 % ilocal", "Invalid operands for %"},
      {"ilocal * dlocal", "Invalid operands for *"},
      {"ilocal / dlocal", "Invalid operands for /"},
      {"ilocal % dlocal", "Invalid operands for %"},
      {"1048577 + dlocal", "Invalid operands for additive expression"},
      {"1048577 - dlocal", "Invalid operands for additive expression"},
      {"ilocal + dlocal", "Invalid operands for additive expression"},
      {"ilocal - dlocal", "Invalid operands for additive expression"},
      {"1048577 << dlocal", "Invalid operands for <<"},
      {"1048577 >> dlocal", "Invalid operands for >>"},
      {"1048577 >>> dlocal", "Invalid operands for >>"},
      {"ilocal << dlocal", "Invalid operands for <<"},
      {"ilocal >> dlocal", "Invalid operands for >>"},
      {"ilocal >>> dlocal", "Invalid operands for >>>"},
      {"1048577 < dlocal", "Invalid operands for <"},
      {"ilocal < dlocal", "Invalid operands for <"},
      {"1048577 > dlocal", "Invalid operands for >"},
      {"ilocal > dlocal", "Invalid operands for >"},
      {"1048577 <= dlocal", "Invalid operands for <="},
      {"ilocal <= dlocal", "Invalid operands for <="},
      {"1048577 >= dlocal", "Invalid operands for >="},
      {"ilocal >= dlocal", "Invalid operands for >="},
      {"1048577 == dlocal", "Invalid operands for =="},
      {"ilocal == dlocal", "Invalid operands for =="},
      /* NOTE: the parser converts a == b to !(a == b). */
      {"1048577 != dlocal", "Invalid operands for =="},
      {"ilocal != dlocal", "Invalid operands for =="},
      {"dlocal & dlocal", "Invalid operands for &"},
      {"1048577 & dlocal", "Invalid operands for &"},
      {"ilocal & dlocal", "Invalid operands for &"},
      {"dlocal | dlocal2", "Invalid operands for |"},
      {"1048577 | dlocal", "Invalid operands for |"},
      {"ilocal | dlocal", "Invalid operands for |"},
      {"dlocal ^ dlocal2", "Invalid operands for ^"},
      {"1048577 ^ dlocal", "Invalid operands for ^"},
      {"ilocal ^ dlocal", "Invalid operands for ^"},
      {"dlocal ? 0 : 1", "Ternary operation condition should be int"},
      {"ilocal ? dlocal : 1", "Type mismatch for ternary operation result"},
      {"ilocal ? 1 : dlocal", "Type mismatch for ternary operation result"},
      {"eval(10)|0", "Invalid asm.js identifier in (forward) function"},
      {"arguments(10)|0", "Invalid asm.js identifier in (forward) function"},
      {"not_a_function(10)|0", "Calling something that's not a function"},
      {"fround(FFI())", "Foreign functions can't return float"},
      {"FFI(fround(0))|0", "Function invocation does not match function type"},
      {"FFI(2147483648)|0", "Function invocation does not match function type"},
      {"d2d(2.0)|0", "Function invocation does not match function type"},
      {"+d2d(2)", "Function invocation does not match function type"},
      {"eval[ilocal & 3]()|0", "Invalid asm.js identifier in (forward)"},
      {"arguments[ilocal & 3]()|0", "Invalid asm.js identifier in (forward)"},
      {"not_a_function[ilocal & 3]()|0", "Identifier does not name a function"},
      {"d2s_tbl[ilocal & 3](0.0)|0", "Function table size does not match"},
      {"+d2s_tbl[ilocal & 1](0.0)", "does not match previous signature"},
      {"d2s_tbl[ilocal & 1](0)|0", "does not match previous signature"},
      {"a.b()|0", "Indirect call index must be in the expr & mask form"},
      {"HEAP32[0][0] = 0", "Invalid heap access"},
      {"heap32[0] = 0", "Undeclared identifier in heap access"},
      {"not_a_function[0] = 0", "Identifier does not represent a heap view"},
      {"HEAP32[0.0] = 0", "Heap access index must be int"},
      {"HEAP32[-1] = 0", "Heap access index must be a 32-bit unsigned integer"},
      {"HEAP32[ilocal >> 1] = 0", "Invalid heap access index"},
      // *VIOLATION* the following is invalid, but because of desugaring it is
      // accepted.
      // {"HEAP32[0 >> 1] = 0", "Invalid heap access index"},
      {"HEAP8[fround(0.0)] = 0", "Invalid heap access index for byte array"},
      {"HEAP8[iish] = 0", "Invalid heap access index for byte array"},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithStdlib(DynamicGlobal("stdlib"))
             ->WithFFI(DynamicGlobal("ffi"))
             ->WithHeap(DynamicGlobal("heap"))
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->WithLocal(DynamicGlobal("ilocal"), iw::AsmType::Int())
             ->WithLocal(DynamicGlobal("dlocal"), iw::AsmType::Double())
             ->WithLocal(DynamicGlobal("dlocal2"), iw::AsmType::Double())
             ->WithLocal(DynamicGlobal("not_a_function"), iw::AsmType::Int())
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithImport(DynamicGlobal("FFI"), iw::AsmTyper::kFFI)
             ->WithGlobal(DynamicGlobal("d2d"), d2d)
             ->WithGlobal(DynamicGlobal("d2s_tbl"), d2s_tbl)
             ->WithGlobal(DynamicGlobal("HEAP32"), iw::AsmType::Int32Array())
             ->WithGlobal(DynamicGlobal("HEAP8"), iw::AsmType::Int8Array())
             ->WithGlobal(DynamicGlobal("a"), nullptr)
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateNumericLiteral) {
  const struct {
    const char* expression;
    iw::AsmType* expected_type;
  } kTests[] = {
      {"0", iw::AsmType::FixNum()},
      {"-1", iw::AsmType::Signed()},
      {"2147483648", iw::AsmType::Unsigned()},
      {"0.0", iw::AsmType::Double()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->SucceedsWithExactType(test->expected_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateIdentifier) {
  const struct {
    const char* expression;
    iw::AsmType* expected_type;
  } kTests[] = {{"afixnum", iw::AsmType::FixNum()},
                {"adouble", iw::AsmType::Double()},
                {"afloat", iw::AsmType::Float()},
                {"anextern", iw::AsmType::Extern()},
                {"avoid", iw::AsmType::Void()}};

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal(test->expression), test->expected_type)
             ->WithGlobal(DynamicGlobal(test->expression),
                          iw::AsmType::Floatish())
             ->SucceedsWithExactType(test->expected_type)) {
      std::cerr << "Test (local identifiers):\n" << test->expression;
      CHECK(false);
    }
  }

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithGlobal(DynamicGlobal(test->expression), test->expected_type)
             ->SucceedsWithExactType(test->expected_type)) {
      std::cerr << "Test (global identifiers):\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateCallExpression) {
  auto v2f = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Float());
    return ret;
  };

  const struct {
    const char* expression;
  } kTests[] = {
      {"a_float_function()"},
      {"fround(0)"},
      {"slocal"},
      {"ulocal"},
      {"dqlocal"},
      {"fishlocal"},
  };

  char full_test[200];
  static const size_t kFullTestSize = arraysize(full_test);
  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    CHECK(v8::base::OS::SNPrintF(full_test, kFullTestSize, "fround(%s)",
                                 test->expression) <
          static_cast<int>(kFullTestSize));
    if (!ValidationOf(Expression(full_test))
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithGlobal(DynamicGlobal("a_float_function"), v2f)
             ->WithLocal(DynamicGlobal("slocal"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("ulocal"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("dqlocal"), iw::AsmType::DoubleQ())
             ->WithLocal(DynamicGlobal("fishlocal"), iw::AsmType::Floatish())
             ->SucceedsWithExactType(iw::AsmType::Float())) {
      std::cerr << "Test:\n" << full_test;
      CHECK(false);
    }
  }

  const struct {
    const char* expression;
    const char* error_message;
  } kFailureTests[] = {
      {"vlocal", "Invalid argument type to fround"},
      {"ilocal", "Invalid argument type to fround"},
      {"a_double_function()", "Function invocation does not match"},
  };

  auto v2d = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Double());
    return ret;
  };

  for (size_t ii = 0; ii < arraysize(kFailureTests); ++ii) {
    const auto* test = kFailureTests + ii;
    CHECK(v8::base::OS::SNPrintF(full_test, kFullTestSize, "fround(%s)",
                                 test->expression) <
          static_cast<int>(kFullTestSize));
    if (!ValidationOf(Expression(full_test))
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithLocal(DynamicGlobal("ilocal"), iw::AsmType::Int())
             ->WithLocal(DynamicGlobal("vlocal"), iw::AsmType::Void())
             ->WithGlobal(DynamicGlobal("a_double_function"), v2d)
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << full_test;
      CHECK(false);
    }
  }
}

TEST(ValidateMemberExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"I8[i]", iw::AsmType::Intish()},  // Legacy: no shift for 8-bit view.
      {"I8[iish >> 0]", iw::AsmType::Intish()},
      {"I8[0]", iw::AsmType::Intish()},
      {"I8[2147483648]", iw::AsmType::Intish()},
      {"U8[iish >> 0]", iw::AsmType::Intish()},
      {"U8[i]", iw::AsmType::Intish()},  // Legacy: no shift for 8-bit view.
      {"U8[0]", iw::AsmType::Intish()},
      {"U8[2147483648]", iw::AsmType::Intish()},
      {"I16[iish >> 1]", iw::AsmType::Intish()},
      {"I16[0]", iw::AsmType::Intish()},
      {"I16[1073741824]", iw::AsmType::Intish()},
      {"U16[iish >> 1]", iw::AsmType::Intish()},
      {"U16[0]", iw::AsmType::Intish()},
      {"U16[1073741824]", iw::AsmType::Intish()},
      {"I32[iish >> 2]", iw::AsmType::Intish()},
      {"I32[0]", iw::AsmType::Intish()},
      {"I32[536870912]", iw::AsmType::Intish()},
      {"U32[iish >> 2]", iw::AsmType::Intish()},
      {"U32[0]", iw::AsmType::Intish()},
      {"U32[536870912]", iw::AsmType::Intish()},
      {"F32[iish >> 2]", iw::AsmType::FloatQ()},
      {"F32[0]", iw::AsmType::FloatQ()},
      {"F32[536870912]", iw::AsmType::FloatQ()},
      {"F64[iish >> 3]", iw::AsmType::DoubleQ()},
      {"F64[0]", iw::AsmType::DoubleQ()},
      {"F64[268435456]", iw::AsmType::DoubleQ()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithGlobal(DynamicGlobal("I8"), iw::AsmType::Int8Array())
             ->WithGlobal(DynamicGlobal("U8"), iw::AsmType::Uint8Array())
             ->WithGlobal(DynamicGlobal("I16"), iw::AsmType::Int16Array())
             ->WithGlobal(DynamicGlobal("U16"), iw::AsmType::Uint16Array())
             ->WithGlobal(DynamicGlobal("I32"), iw::AsmType::Int32Array())
             ->WithGlobal(DynamicGlobal("U32"), iw::AsmType::Uint32Array())
             ->WithGlobal(DynamicGlobal("F32"), iw::AsmType::Float32Array())
             ->WithGlobal(DynamicGlobal("F64"), iw::AsmType::Float64Array())
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->WithLocal(DynamicGlobal("i"), iw::AsmType::Int())
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateAssignmentExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      // -----------------------------------------------------------------------
      // Array assignments.
      // Storing signed to int heap view.
      {"I8[1024] = -1024", iw::AsmType::Signed()},
      {"I8[1024 >> 0] = -1024", iw::AsmType::Signed()},
      {"I8[0] = -1024", iw::AsmType::Signed()},
      {"I8[2147483648] = -1024", iw::AsmType::Signed()},
      {"U8[1024 >> 0] = -1024", iw::AsmType::Signed()},
      {"U8[0] = -1024", iw::AsmType::Signed()},
      {"U8[2147483648] = -1024", iw::AsmType::Signed()},
      {"I16[1024 >> 1] = -1024", iw::AsmType::Signed()},
      {"I16[0] = -1024", iw::AsmType::Signed()},
      {"I16[1073741824] = -1024", iw::AsmType::Signed()},  // not pre-shifted.
      {"U16[1024 >> 1] = -1024", iw::AsmType::Signed()},
      {"U16[0] = -1024", iw::AsmType::Signed()},
      {"U16[1073741824] = -1024", iw::AsmType::Signed()},  // not pre-shifted.
      {"I32[1024 >> 2] = -1024", iw::AsmType::Signed()},
      {"I32[0] = -1024", iw::AsmType::Signed()},
      {"I32[536870912] = -1024", iw::AsmType::Signed()},  // not pre-shifted.
      {"U32[1024 >> 2] = -1024", iw::AsmType::Signed()},
      {"U32[0] = -1024", iw::AsmType::Signed()},
      {"U32[536870912] = -1024", iw::AsmType::Signed()},  // not pre-shifted.
      // Sroting fixnum to int heap view.
      {"I8[1024] = 1024", iw::AsmType::FixNum()},
      {"I8[1024 >> 0] = 1024", iw::AsmType::FixNum()},
      {"I8[0] = 1024", iw::AsmType::FixNum()},
      {"I8[2147483648] = 1024", iw::AsmType::FixNum()},
      {"U8[1024 >> 0] = 1024", iw::AsmType::FixNum()},
      {"U8[0] = 1024", iw::AsmType::FixNum()},
      {"U8[2147483648] = 1024", iw::AsmType::FixNum()},
      {"I16[1024 >> 1] = 1024", iw::AsmType::FixNum()},
      {"I16[0] = 1024", iw::AsmType::FixNum()},
      {"I16[1073741824] = 1024", iw::AsmType::FixNum()},  // not pre-shifted.
      {"U16[1024 >> 1] = 1024", iw::AsmType::FixNum()},
      {"U16[0] = 1024", iw::AsmType::FixNum()},
      {"U16[1073741824] = 1024", iw::AsmType::FixNum()},  // not pre-shifted.
      {"I32[1024 >> 2] = 1024", iw::AsmType::FixNum()},
      {"I32[0] = 1024", iw::AsmType::FixNum()},
      {"I32[536870912] = 1024", iw::AsmType::FixNum()},  // not pre-shifted.
      {"U32[1024 >> 2] = 1024", iw::AsmType::FixNum()},
      {"U32[0] = 1024", iw::AsmType::FixNum()},
      {"U32[536870912] = 1024", iw::AsmType::FixNum()},  // not pre-shifted.
      // Storing int to int heap view.
      {"I8[ilocal] = ilocal", iw::AsmType::Int()},
      {"I8[ilocal >> 0] = ilocal", iw::AsmType::Int()},
      {"I8[0] = ilocal", iw::AsmType::Int()},
      {"I8[2147483648] = ilocal", iw::AsmType::Int()},
      {"U8[ilocal >> 0] = ilocal", iw::AsmType::Int()},
      {"U8[0] = ilocal", iw::AsmType::Int()},
      {"U8[2147483648] = ilocal", iw::AsmType::Int()},
      {"I16[ilocal >> 1] = ilocal", iw::AsmType::Int()},
      {"I16[0] = ilocal", iw::AsmType::Int()},
      {"I16[1073741824] = ilocal", iw::AsmType::Int()},  // not pre-shifted.
      {"U16[ilocal >> 1] = ilocal", iw::AsmType::Int()},
      {"U16[0] = ilocal", iw::AsmType::Int()},
      {"U16[1073741824] = ilocal", iw::AsmType::Int()},  // not pre-shifted.
      {"I32[ilocal >> 2] = ilocal", iw::AsmType::Int()},
      {"I32[0] = ilocal", iw::AsmType::Int()},
      {"I32[536870912] = ilocal", iw::AsmType::Int()},  // not pre-shifted.
      {"U32[ilocal >> 2] = ilocal", iw::AsmType::Int()},
      {"U32[0] = ilocal", iw::AsmType::Int()},
      {"U32[536870912] = ilocal", iw::AsmType::Int()},  // not pre-shifted.
      // Storing intish to int heap view.
      {"I8[ilocal] = iish", iw::AsmType::Intish()},
      {"I8[iish >> 0] = iish", iw::AsmType::Intish()},
      {"I8[0] = iish", iw::AsmType::Intish()},
      {"I8[2147483648] = iish", iw::AsmType::Intish()},
      {"U8[iish >> 0] = iish", iw::AsmType::Intish()},
      {"U8[0] = iish", iw::AsmType::Intish()},
      {"U8[2147483648] = iish", iw::AsmType::Intish()},
      {"I16[iish >> 1] = iish", iw::AsmType::Intish()},
      {"I16[0] = iish", iw::AsmType::Intish()},
      {"I16[1073741824] = iish", iw::AsmType::Intish()},  // not pre-shifted.
      {"U16[iish >> 1] = iish", iw::AsmType::Intish()},
      {"U16[0] = iish", iw::AsmType::Intish()},
      {"U16[1073741824] = iish", iw::AsmType::Intish()},  // not pre-shifted.
      {"I32[iish >> 2] = iish", iw::AsmType::Intish()},
      {"I32[0] = iish", iw::AsmType::Intish()},
      {"I32[536870912] = iish", iw::AsmType::Intish()},  // not pre-shifted.
      {"U32[iish >> 2] = iish", iw::AsmType::Intish()},
      {"U32[0] = iish", iw::AsmType::Intish()},
      {"U32[536870912] = iish", iw::AsmType::Intish()},  // not pre-shifted.
      // Storing floatish to f32 heap view.
      {"F32[iish >> 2] = fish", iw::AsmType::Floatish()},
      {"F32[0] = fish", iw::AsmType::Floatish()},
      {"F32[536870912] = fish ", iw::AsmType::Floatish()},  // not pre-shifted.
      // Storing double? to f32 heap view.
      {"F32[iish >> 2] = dq", iw::AsmType::DoubleQ()},
      {"F32[0] = dq", iw::AsmType::DoubleQ()},
      {"F32[536870912] = dq", iw::AsmType::DoubleQ()},  // not pre-shifted.
      // Storing float? to f64 heap view.
      {"F64[iish >> 3] = fq", iw::AsmType::FloatQ()},
      {"F64[0] = fq", iw::AsmType::FloatQ()},
      {"F64[268435456] = fq", iw::AsmType::FloatQ()},  // not pre-shifted.
      // Storing double? to f64 heap view.
      {"F64[iish >> 3] = dq", iw::AsmType::DoubleQ()},
      {"F64[0] = dq", iw::AsmType::DoubleQ()},
      {"F64[268435456] = dq", iw::AsmType::DoubleQ()},  // not pre-shifted.
      // -----------------------------------------------------------------------
      // Scalar assignments.
      {"ilocal = 1024", iw::AsmType::FixNum()},
      {"ilocal = -1024", iw::AsmType::Signed()},
      {"ilocal = 2147483648", iw::AsmType::Unsigned()},
      {"ilocal = iglobal", iw::AsmType::Int()},
      {"iglobal = 1024", iw::AsmType::FixNum()},
      {"iglobal = -1024", iw::AsmType::Signed()},
      {"iglobal = 2147483648", iw::AsmType::Unsigned()},
      {"iglobal = ilocal", iw::AsmType::Int()},
      {"dlocal = 0.0", iw::AsmType::Double()},
      {"dlocal = +make_double()", iw::AsmType::Double()},
      {"dglobal = 0.0", iw::AsmType::Double()},
      {"dglobal = +make_double()", iw::AsmType::Double()},
      {"flocal = fround(0)", iw::AsmType::Float()},
      {"flocal = fround(make_float())", iw::AsmType::Float()},
      {"fglobal = fround(0)", iw::AsmType::Float()},
      {"fglobal = fround(make_float())", iw::AsmType::Float()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithLocal(DynamicGlobal("fq"), iw::AsmType::FloatQ())
             ->WithLocal(DynamicGlobal("dq"), iw::AsmType::DoubleQ())
             ->WithLocal(DynamicGlobal("fish"), iw::AsmType::Floatish())
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->WithGlobal(DynamicGlobal("iglobal"), iw::AsmType::Int())
             ->WithGlobal(DynamicGlobal("dglobal"), iw::AsmType::Double())
             ->WithGlobal(DynamicGlobal("fglobal"), iw::AsmType::Float())
             ->WithLocal(DynamicGlobal("ilocal"), iw::AsmType::Int())
             ->WithLocal(DynamicGlobal("dlocal"), iw::AsmType::Double())
             ->WithLocal(DynamicGlobal("flocal"), iw::AsmType::Float())
             ->WithGlobal(DynamicGlobal("I8"), iw::AsmType::Int8Array())
             ->WithGlobal(DynamicGlobal("U8"), iw::AsmType::Uint8Array())
             ->WithGlobal(DynamicGlobal("I16"), iw::AsmType::Int16Array())
             ->WithGlobal(DynamicGlobal("U16"), iw::AsmType::Uint16Array())
             ->WithGlobal(DynamicGlobal("I32"), iw::AsmType::Int32Array())
             ->WithGlobal(DynamicGlobal("U32"), iw::AsmType::Uint32Array())
             ->WithGlobal(DynamicGlobal("F32"), iw::AsmType::Float32Array())
             ->WithGlobal(DynamicGlobal("F64"), iw::AsmType::Float64Array())
             ->WithGlobal(DynamicGlobal("make_float"), nullptr)
             ->WithGlobal(DynamicGlobal("make_double"), nullptr)
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateUnaryExpression) {
  auto v2d = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Double());
    return ret;
  };

  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"-2147483648", iw::AsmType::Signed()},
      {"-1024", iw::AsmType::Signed()},
      {"-1", iw::AsmType::Signed()},
      {"-2147483648.0", iw::AsmType::Double()},
      {"+make_double()", iw::AsmType::Double()},
      {"+dbl()", iw::AsmType::Double()},
      {"make_double() * 1.0", iw::AsmType::Double()},  // Violation.
      {"~~fq", iw::AsmType::Signed()},
      {"~~dglobal", iw::AsmType::Signed()},
      {"+slocal", iw::AsmType::Double()},
      {"slocal * 1.0", iw::AsmType::Double()},  // Violation.
      {"+ulocal", iw::AsmType::Double()},
      {"ulocal * 1.0", iw::AsmType::Double()},  // Violation.
      {"+dq", iw::AsmType::Double()},
      {"dq * 1.0", iw::AsmType::Double()},  // Violation.
      {"+fq", iw::AsmType::Double()},
      {"fq * 1.0", iw::AsmType::Double()},  // Violation.
      {"-ilocal", iw::AsmType::Intish()},
      {"ilocal * -1", iw::AsmType::Intish()},  // Violation.
      {"-dq", iw::AsmType::Double()},
      {"dq * -1", iw::AsmType::Double()},  // Violation.
      {"-fq", iw::AsmType::Floatish()},
      {"fq * -1", iw::AsmType::Floatish()},  // Violation.
      {"~iish", iw::AsmType::Signed()},
      {"iish ^ -1", iw::AsmType::Signed()},  // Violation, but OK.
      {"!ilocal", iw::AsmType::Int()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal("fq"), iw::AsmType::FloatQ())
             ->WithLocal(DynamicGlobal("dq"), iw::AsmType::DoubleQ())
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->WithLocal(DynamicGlobal("slocal"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("ulocal"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("ilocal"), iw::AsmType::Int())
             ->WithGlobal(DynamicGlobal("dglobal"), iw::AsmType::Double())
             ->WithGlobal(DynamicGlobal("make_double"), nullptr)
             ->WithGlobal(DynamicGlobal("dbl"), v2d)
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateMultiplicativeExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"dq * dq", iw::AsmType::Double()},
      {"fq * fq", iw::AsmType::Floatish()},
      {"slocal / slocal", iw::AsmType::Intish()},
      {"ulocal / ulocal", iw::AsmType::Intish()},
      {"dq / dq", iw::AsmType::Double()},
      {"fq / fq", iw::AsmType::Floatish()},
      {"slocal % slocal", iw::AsmType::Intish()},
      {"ulocal % ulocal", iw::AsmType::Intish()},
      {"dq % dq", iw::AsmType::Double()},
      {"-1048575 * ilocal", iw::AsmType::Intish()},
      {"ilocal * -1048575", iw::AsmType::Intish()},
      {"1048575 * ilocal", iw::AsmType::Intish()},
      {"ilocal * 1048575", iw::AsmType::Intish()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal("fq"), iw::AsmType::FloatQ())
             ->WithLocal(DynamicGlobal("dq"), iw::AsmType::DoubleQ())
             ->WithLocal(DynamicGlobal("slocal"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("ulocal"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("ilocal"), iw::AsmType::Int())
             ->WithGlobal(DynamicGlobal("dglobal"), iw::AsmType::Double())
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateAdditiveExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"dlocal + dlocal", iw::AsmType::Double()},
      {"fq + fq", iw::AsmType::Floatish()},
      {"dq - dq", iw::AsmType::Double()},
      {"fq - fq", iw::AsmType::Floatish()},
      {"ilocal + 1", iw::AsmType::Intish()},
      {"ilocal - 1", iw::AsmType::Intish()},
      {"slocal + ilocal + 1", iw::AsmType::Intish()},
      {"slocal - ilocal + 1", iw::AsmType::Intish()},
      {"ulocal + ilocal + 1", iw::AsmType::Intish()},
      {"ulocal - ilocal + 1", iw::AsmType::Intish()},
      {"ulocal + slocal + ilocal + 1", iw::AsmType::Intish()},
      {"ulocal + slocal - ilocal + 1", iw::AsmType::Intish()},
      {"ulocal - slocal + ilocal + 1", iw::AsmType::Intish()},
      {"ulocal - slocal - ilocal + 1", iw::AsmType::Intish()},
      {"1 + 1", iw::AsmType::FixNum()},  // Violation: intish.
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal("fq"), iw::AsmType::FloatQ())
             ->WithLocal(DynamicGlobal("dq"), iw::AsmType::DoubleQ())
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->WithLocal(DynamicGlobal("dlocal"), iw::AsmType::Double())
             ->WithLocal(DynamicGlobal("slocal"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("ulocal"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("ilocal"), iw::AsmType::Int())
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateShiftExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"iish << iish", iw::AsmType::Signed()},
      {"iish >> iish", iw::AsmType::Signed()},
      {"iish >>> iish", iw::AsmType::Unsigned()},
      {"1 << 0", iw::AsmType::FixNum()},  // Violation: signed.
      {"1 >> 0", iw::AsmType::FixNum()},  // Violation: signed.
      {"4294967295 >>> 0", iw::AsmType::Unsigned()},
      {"-1 >>> 0", iw::AsmType::Unsigned()},
      {"2147483647 >>> 0", iw::AsmType::FixNum()},  // Violation: unsigned.
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateComparisonExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      // -----------------------------------------------------------------------
      // Non const <op> Non const
      {"s0 == s1", iw::AsmType::Int()},
      {"u0 == u1", iw::AsmType::Int()},
      {"f0 == f1", iw::AsmType::Int()},
      {"d0 == d1", iw::AsmType::Int()},
      {"s0 != s1", iw::AsmType::Int()},
      {"u0 != u1", iw::AsmType::Int()},
      {"f0 != f1", iw::AsmType::Int()},
      {"d0 != d1", iw::AsmType::Int()},
      {"s0 < s1", iw::AsmType::Int()},
      {"u0 < u1", iw::AsmType::Int()},
      {"f0 < f1", iw::AsmType::Int()},
      {"d0 < d1", iw::AsmType::Int()},
      {"s0 <= s1", iw::AsmType::Int()},
      {"u0 <= u1", iw::AsmType::Int()},
      {"f0 <= f1", iw::AsmType::Int()},
      {"d0 <= d1", iw::AsmType::Int()},
      {"s0 > s1", iw::AsmType::Int()},
      {"u0 > u1", iw::AsmType::Int()},
      {"f0 > f1", iw::AsmType::Int()},
      {"d0 > d1", iw::AsmType::Int()},
      {"s0 >= s1", iw::AsmType::Int()},
      {"u0 >= u1", iw::AsmType::Int()},
      {"f0 >= f1", iw::AsmType::Int()},
      {"d0 >= d1", iw::AsmType::Int()},
      // -----------------------------------------------------------------------
      // Non const <op> Const
      {"s0 == -1025", iw::AsmType::Int()},
      {"u0 == 123456789", iw::AsmType::Int()},
      {"f0 == fround(123456.78)", iw::AsmType::Int()},
      {"d0 == 9876543.201", iw::AsmType::Int()},
      {"s0 != -1025", iw::AsmType::Int()},
      {"u0 != 123456789", iw::AsmType::Int()},
      {"f0 != fround(123456.78)", iw::AsmType::Int()},
      {"d0 != 9876543.201", iw::AsmType::Int()},
      {"s0 < -1025", iw::AsmType::Int()},
      {"u0 < 123456789", iw::AsmType::Int()},
      {"f0 < fround(123456.78)", iw::AsmType::Int()},
      {"d0 < 9876543.201", iw::AsmType::Int()},
      {"s0 <= -1025", iw::AsmType::Int()},
      {"u0 <= 123456789", iw::AsmType::Int()},
      {"f0 <= fround(123456.78)", iw::AsmType::Int()},
      {"d0 <= 9876543.201", iw::AsmType::Int()},
      {"s0 > -1025", iw::AsmType::Int()},
      {"u0 > 123456789", iw::AsmType::Int()},
      {"f0 > fround(123456.78)", iw::AsmType::Int()},
      {"d0 > 9876543.201", iw::AsmType::Int()},
      {"s0 >= -1025", iw::AsmType::Int()},
      {"u0 >= 123456789", iw::AsmType::Int()},
      {"f0 >= fround(123456.78)", iw::AsmType::Int()},
      {"d0 >= 9876543.201", iw::AsmType::Int()},
      // -----------------------------------------------------------------------
      // Const <op> Non const
      {"-1025 == s0", iw::AsmType::Int()},
      {"123456789 == u0", iw::AsmType::Int()},
      {"fround(123456.78) == f0", iw::AsmType::Int()},
      {"9876543.201 == d0", iw::AsmType::Int()},
      {"-1025 != s0", iw::AsmType::Int()},
      {"123456789 != u0", iw::AsmType::Int()},
      {"fround(123456.78) != f0", iw::AsmType::Int()},
      {"9876543.201 != d0", iw::AsmType::Int()},
      {"-1025 < s0", iw::AsmType::Int()},
      {"123456789 < u0", iw::AsmType::Int()},
      {"fround(123456.78) < f0", iw::AsmType::Int()},
      {"9876543.201 < d0", iw::AsmType::Int()},
      {"-1025 <= s0", iw::AsmType::Int()},
      {"123456789 <= u0", iw::AsmType::Int()},
      {"fround(123456.78) <= f0", iw::AsmType::Int()},
      {"9876543.201 <= d0", iw::AsmType::Int()},
      {"-1025 > s0", iw::AsmType::Int()},
      {"123456789 > u0", iw::AsmType::Int()},
      {"fround(123456.78) > f0", iw::AsmType::Int()},
      {"9876543.201 > d0", iw::AsmType::Int()},
      {"-1025 >= s0", iw::AsmType::Int()},
      {"123456789 >= u0", iw::AsmType::Int()},
      {"fround(123456.78) >= f0", iw::AsmType::Int()},
      {"9876543.201 >= d0", iw::AsmType::Int()},
      // TODO(jpp): maybe add Const <op> Const.
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithLocal(DynamicGlobal("u0"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("u1"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("s0"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("s1"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("f0"), iw::AsmType::Float())
             ->WithLocal(DynamicGlobal("f1"), iw::AsmType::Float())
             ->WithLocal(DynamicGlobal("d0"), iw::AsmType::Double())
             ->WithLocal(DynamicGlobal("d1"), iw::AsmType::Double())
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateBitwiseExpression) {
  auto v2s = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Signed());
    return ret;
  };

  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"iish0 & iish1", iw::AsmType::Signed()},
      {"iish0 | iish1", iw::AsmType::Signed()},
      {"iish0 ^ iish1", iw::AsmType::Signed()},
      {"iish0 & -1", iw::AsmType::Signed()},
      {"iish0 | -1", iw::AsmType::Signed()},
      {"iish0 ^ -1", iw::AsmType::Signed()},
      {"2147483648 & iish1", iw::AsmType::Signed()},
      {"2147483648 | iish1", iw::AsmType::Signed()},
      {"2147483648 ^ iish1", iw::AsmType::Signed()},
      {"2147483648 & 0", iw::AsmType::FixNum()},  // Violation: signed.
      {"2147483648 | 0", iw::AsmType::Signed()},
      {"2147483648 ^ 0", iw::AsmType::Signed()},
      {"2134651 & 123", iw::AsmType::FixNum()},  // Violation: signed.
      {"2134651 | 123", iw::AsmType::FixNum()},  // Violation: signed.
      {"2134651 ^ 123", iw::AsmType::FixNum()},  // Violation: signed.
      {"make_signed()|0", iw::AsmType::Signed()},
      {"signed()|0", iw::AsmType::Signed()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal("iish1"), iw::AsmType::Intish())
             ->WithLocal(DynamicGlobal("iish0"), iw::AsmType::Intish())
             ->WithGlobal(DynamicGlobal("signed"), v2s)
             ->WithGlobal(DynamicGlobal("make_signed"), nullptr)
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateConditionalExpression) {
  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      {"i0 ? i0 : i1", iw::AsmType::Int()},
      {"i0 ? f0 : f1", iw::AsmType::Float()},
      {"i0 ? d0 : d1", iw::AsmType::Double()},
      {"0 ? -1 : 2147483648", iw::AsmType::Int()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithLocal(DynamicGlobal("i0"), iw::AsmType::Int())
             ->WithLocal(DynamicGlobal("i1"), iw::AsmType::Int())
             ->WithLocal(DynamicGlobal("f0"), iw::AsmType::Float())
             ->WithLocal(DynamicGlobal("f1"), iw::AsmType::Float())
             ->WithLocal(DynamicGlobal("d0"), iw::AsmType::Double())
             ->WithLocal(DynamicGlobal("d1"), iw::AsmType::Double())
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(ValidateCall) {
  auto v2f = [](Zone* zone) -> iw::AsmType* {
    auto* ret = iw::AsmType::Function(zone, iw::AsmType::Float());
    return ret;
  };

  // ifd2_ is a helper function that returns a lambda for creating a function
  // type that accepts an int, a float, and a double. ret_type_factory is a
  // pointer to an AsmType*() function, and (*ret_type_factory)() returns the
  // desired return type. For example,
  //
  // ifd2_(&iw::AsmType::Float)
  //
  // returns an AsmType representing an asm.js function with the following
  // signature:
  //
  // float(int, float, double)
  auto ifd2_ = [](iw::AsmType* (
      *ret_type_factory)()) -> std::function<iw::AsmType*(Zone*)> {
    return [ret_type_factory](Zone* zone) -> iw::AsmType* {
      auto* ret = iw::AsmType::Function(zone, (*ret_type_factory)());
      ret->AsFunctionType()->AddArgument(iw::AsmType::Int());
      ret->AsFunctionType()->AddArgument(iw::AsmType::Float());
      ret->AsFunctionType()->AddArgument(iw::AsmType::Double());
      return ret;
    };
  };
  auto ifd2f = ifd2_(&iw::AsmType::Float);
  auto ifd2d = ifd2_(&iw::AsmType::Double);
  auto ifd2i = ifd2_(&iw::AsmType::Signed);

  // Just like ifd2_, but this one returns a type representing a function table.
  auto tbl_ifd2_ = [](size_t tbl_size, iw::AsmType* (*ret_type_factory)())
      -> std::function<iw::AsmType*(Zone*)> {
        return [tbl_size, ret_type_factory](Zone* zone) -> iw::AsmType* {
          auto* signature = iw::AsmType::Function(zone, (*ret_type_factory)());
          signature->AsFunctionType()->AddArgument(iw::AsmType::Int());
          signature->AsFunctionType()->AddArgument(iw::AsmType::Float());
          signature->AsFunctionType()->AddArgument(iw::AsmType::Double());

          auto* ret = iw::AsmType::FunctionTableType(zone, tbl_size, signature);
          return ret;
        };
      };
  auto ifd2f_tbl = tbl_ifd2_(32, &iw::AsmType::Float);
  auto ifd2d_tbl = tbl_ifd2_(64, &iw::AsmType::Double);
  auto ifd2i_tbl = tbl_ifd2_(4096, &iw::AsmType::Signed);

  const struct {
    const char* expression;
    iw::AsmType* load_type;
  } kTests[] = {
      // -----------------------------------------------------------------------
      // Functions.
      {"fround(v2f())", iw::AsmType::Float()},
      {"fround(fish)", iw::AsmType::Float()},
      {"fround(dq)", iw::AsmType::Float()},
      {"fround(s)", iw::AsmType::Float()},
      {"fround(u)", iw::AsmType::Float()},
      {"ffi()|0", iw::AsmType::Signed()},
      {"ffi(1.0)|0", iw::AsmType::Signed()},
      {"ffi(1.0, 2.0)|0", iw::AsmType::Signed()},
      {"ffi(1.0, 2.0, 3)|0", iw::AsmType::Signed()},
      {"ffi(1.0, 2.0, 3, 4)|0", iw::AsmType::Signed()},
      {"+ffi()", iw::AsmType::Double()},
      {"+ffi(1.0)", iw::AsmType::Double()},
      {"+ffi(1.0, 2.0)", iw::AsmType::Double()},
      {"+ffi(1.0, 2.0, 3)", iw::AsmType::Double()},
      {"+ffi(1.0, 2.0, 3, 4)", iw::AsmType::Double()},
      {"fround(ifd2f(1, fround(1), 1.0))", iw::AsmType::Float()},
      {"+ifd2d(1, fround(1), 1.0)", iw::AsmType::Double()},
      {"ifd2i(1, fround(1), 1.0)|0", iw::AsmType::Signed()},
      // -----------------------------------------------------------------------
      // Function tables.
      {"fround(ifd2f_tbl[iish & 31](1, fround(1), 1.0))", iw::AsmType::Float()},
      {"+ifd2d_tbl[iish & 63](1, fround(1), 1.0)", iw::AsmType::Double()},
      {"ifd2i_tbl[iish & 4095](1, fround(1), 1.0)|0", iw::AsmType::Signed()},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Expression(test->expression))
             ->WithImport(DynamicGlobal("fround"), iw::AsmTyper::kMathFround)
             ->WithImport(DynamicGlobal("ffi"), iw::AsmTyper::kFFI)
             ->WithLocal(DynamicGlobal("fish"), iw::AsmType::Floatish())
             ->WithLocal(DynamicGlobal("dq"), iw::AsmType::DoubleQ())
             ->WithLocal(DynamicGlobal("s"), iw::AsmType::Signed())
             ->WithLocal(DynamicGlobal("u"), iw::AsmType::Unsigned())
             ->WithLocal(DynamicGlobal("iish"), iw::AsmType::Intish())
             ->WithGlobal(DynamicGlobal("v2f"), v2f)
             ->WithGlobal(DynamicGlobal("ifd2f"), nullptr)
             ->WithGlobal(DynamicGlobal("ifd2d"), nullptr)
             ->WithGlobal(DynamicGlobal("ifd2i"), nullptr)
             ->WithGlobal(DynamicGlobal("ifd2f_tbl"), ifd2f_tbl)
             ->WithGlobal(DynamicGlobal("ifd2d_tbl"), ifd2d_tbl)
             ->WithGlobal(DynamicGlobal("ifd2i_tbl"), ifd2i_tbl)
             ->SucceedsWithExactType(test->load_type)) {
      std::cerr << "Test:\n" << test->expression;
      CHECK(false);
    }
  }
}

TEST(CannotReferenceModuleName) {
  v8::V8::Initialize();

  const struct {
    const char* module;
    const char* error_message;
  } kTests[] = {
      {"function asm() {\n"
       "  'use asm';\n"
       "  function f() { asm; }\n"
       "}",
       "accessed by ordinary expressions"},
      {"function asm() { 'use asm'; return asm; }", "Module cannot export"},
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    const auto* test = kTests + ii;
    if (!ValidationOf(Module(test->module))
             ->FailsWithMessage(test->error_message)) {
      std::cerr << "Test:\n" << test->module;
      CHECK(false);
    }
  }
}

TEST(InvalidSourceLayout) {
  const char* kTests[] = {
      "function asm() {\n"
      "  'use asm';\n"
      "  function f() {}\n"
      "  var v = 0;\n"
      "  var v_v = [f];\n"
      "  return f;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  function f() {}\n"
      "  var v_v = [f];\n"
      "  var v = 0;\n"
      "  return f;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  function f() {}\n"
      "  var v_v = [f];\n"
      "  return f;\n"
      "  var v = 0;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  var v = 0;\n"
      "  var v_v = [f];\n"
      "  function f() {}\n"
      "  return f;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  var v = 0;\n"
      "  var v_v = [f];\n"
      "  return f;\n"
      "  function f() {}\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  var v = 0;\n"
      "  function f() {}\n"
      "  return f;\n"
      "  var v_v = [f];\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  var v = 0;\n"
      "  function f() {}\n"
      "  var v1 = 0;\n"
      "  var v_v = [f];\n"
      "  return f;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  var v = 0;\n"
      "  function f() {}\n"
      "  var v_v = [f];\n"
      "  var v1 = 0;\n"
      "  return f;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  var v = 0;\n"
      "  function f() {}\n"
      "  var v_v = [f];\n"
      "  return f;\n"
      "  var v1 = 0;\n"
      "}",
      "function asm() {\n"
      "  function f() {}\n"
      "  'use asm';\n"
      "  var v_v = [f];\n"
      "  return f;\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  return f;\n"
      "  var v = 0;\n"
      "  function f() {}\n"
      "  var v_v = [f];\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  return f;\n"
      "  function f() {}\n"
      "}",
      "function __f_59() {\n"
      "  'use asm';\n"
      "  function __f_110() {\n"
      "    return 71;\n"
      "  }\n"
      "  function __f_21() {\n"
      "    var __v_38 = 0;\n"
      "    return __v_23[__v_38&0]() | 0;\n"
      "  }\n"
      "  return {__f_21:__f_21};\n"
      "  var __v_23 = [__f_110];\n"
      "}",
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    if (!ValidationOf(Module(kTests[ii]))
             ->FailsWithMessage("Invalid asm.js source code layout")) {
      std::cerr << "Test:\n" << kTests[ii];
      CHECK(false);
    }
  }
}

// This issue was triggered because of the "lenient" 8-bit heap access code
// path. The canonical heap access index validation fails because __34 is not an
// intish. Then, during the "lenient" code path for accessing elements in 8-bit
// heap views, the __34 node in the indexing expression would be re-tagged, thus
// causing the assertion failure.
TEST(B63099) {
  const char* kTests[] = {
      "function __f_109(stdlib, __v_36, buffer) {\n"
      "  'use asm';\n"
      "  var __v_34 = new stdlib.Uint8Array(buffer);\n"
      "  function __f_22() {__v_34[__v_34>>0]|0 + 1 | 0;\n"
      "  }\n"
      "}",
      "function __f_109(stdlib, __v_36, buffer) {\n"
      "  'use asm';\n"
      "  var __v_34 = new stdlib.Int8Array(buffer);\n"
      "  function __f_22() {__v_34[__v_34>>0]|0 + 1 | 0;\n"
      "  }\n"
      "}",
  };

  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    if (!ValidationOf(Module(kTests[ii]))
             ->FailsWithMessage("Invalid heap access index")) {
      std::cerr << "Test:\n" << kTests[ii];
      CHECK(false);
    }
  }
}

// This issue was triggered because assignments to immutable symbols (e.g., the
// module's name, or any of the asm.js' module parameters) was not being
// handled.
TEST(B640194) {
  const char* kTests[] = {
      "function asm() {\n"
      "  'use asm';\n"
      "  function f() {\n"
      "    asm = 0;\n"
      "  }\n"
      "  return f;\n"
      "}",
      "function asm(stdlib) {\n"
      "  'use asm';\n"
      "  function f() {\n"
      "    stdlib = 0;\n"
      "  }\n"
      "  return f;\n"
      "}",
      "function asm(stdlib, foreign) {\n"
      "  'use asm';\n"
      "  function f() {\n"
      "    foreign = 0;\n"
      "  }\n"
      "  return f;\n"
      "}",
      "function asm(stdlib, foreign, heap) {\n"
      "  'use asm';\n"
      "  function f() {\n"
      "    heap = 0;\n"
      "  }\n"
      "  return f;\n"
      "}",
      "function asm(stdlib, foreign, heap) {\n"
      "  'use asm';\n"
      "  var f = stdlib.Math.fround;\n"
      "  function f() {\n"
      "    f = 0;\n"
      "  }\n"
      "  return f;\n"
      "}",
      "function asm(stdlib, foreign, heap) {\n"
      "  'use asm';\n"
      "  var E = stdlib.Math.E;\n"
      "  function f() {\n"
      "    E = 0;\n"
      "  }\n"
      "  return f;\n"
      "}",
  };
  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    if (!ValidationOf(Module(kTests[ii]))
             ->FailsWithMessage("Can't assign to immutable symbol")) {
      std::cerr << "Test:\n" << kTests[ii];
      CHECK(false);
    }
  }
}

TEST(B660813) {
  const char* kTests[] = {
      "function asm() {\n"
      "  'use asm';\n"
      "  const i = 0xffffffff;\n"
      "  function f() {\n"
      "    return i;\n"
      "  }\n"
      "}",
      "function asm() {\n"
      "  'use asm';\n"
      "  const i = -(-2147483648);\n"
      "  function f() {\n"
      "    return i;\n"
      "  }\n"
      "}",
  };
  for (size_t ii = 0; ii < arraysize(kTests); ++ii) {
    if (!ValidationOf(Module(kTests[ii]))
             ->FailsWithMessage(
                 "Constant in return must be signed, float, or double.")) {
      std::cerr << "Test:\n" << kTests[ii];
      CHECK(false);
    }
  }
}

}  // namespace
