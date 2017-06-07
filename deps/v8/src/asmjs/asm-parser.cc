// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-parser.h"

// Required to get M_E etc. for MSVC.
// References from STDLIB_MATH_VALUE_LIST in asm-names.h
#if defined(_WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <string.h>

#include <algorithm>

#include "src/asmjs/asm-types.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

#ifdef DEBUG
#define FAIL_AND_RETURN(ret, msg)                                        \
  failed_ = true;                                                        \
  failure_message_ = msg;                                                \
  failure_location_ = scanner_.GetPosition();                            \
  if (FLAG_trace_asm_parser) {                                           \
    PrintF("[asm.js failure: %s, token: '%s', see: %s:%d]\n", msg,       \
           scanner_.Name(scanner_.Token()).c_str(), __FILE__, __LINE__); \
  }                                                                      \
  return ret;
#else
#define FAIL_AND_RETURN(ret, msg)             \
  failed_ = true;                             \
  failure_message_ = msg;                     \
  failure_location_ = scanner_.GetPosition(); \
  return ret;
#endif

#define FAIL(msg) FAIL_AND_RETURN(, msg)
#define FAILn(msg) FAIL_AND_RETURN(nullptr, msg)
#define FAILf(msg) FAIL_AND_RETURN(false, msg)

#define EXPECT_TOKEN_OR_RETURN(ret, token)      \
  do {                                          \
    if (scanner_.Token() != token) {            \
      FAIL_AND_RETURN(ret, "Unexpected token"); \
    }                                           \
    scanner_.Next();                            \
  } while (false)

#define EXPECT_TOKEN(token) EXPECT_TOKEN_OR_RETURN(, token)
#define EXPECT_TOKENn(token) EXPECT_TOKEN_OR_RETURN(nullptr, token)
#define EXPECT_TOKENf(token) EXPECT_TOKEN_OR_RETURN(false, token)

#define RECURSE_OR_RETURN(ret, call)                                       \
  do {                                                                     \
    DCHECK(!failed_);                                                      \
    if (GetCurrentStackPosition() < stack_limit_) {                        \
      FAIL_AND_RETURN(ret, "Stack overflow while parsing asm.js module."); \
    }                                                                      \
    call;                                                                  \
    if (failed_) return ret;                                               \
  } while (false)

#define RECURSE(call) RECURSE_OR_RETURN(, call)
#define RECURSEn(call) RECURSE_OR_RETURN(nullptr, call)
#define RECURSEf(call) RECURSE_OR_RETURN(false, call)

#define TOK(name) AsmJsScanner::kToken_##name

AsmJsParser::AsmJsParser(Isolate* isolate, Zone* zone, Handle<Script> script,
                         int start, int end)
    : zone_(zone),
      module_builder_(new (zone) WasmModuleBuilder(zone)),
      return_type_(nullptr),
      stack_limit_(isolate->stack_guard()->real_climit()),
      global_var_info_(zone),
      local_var_info_(zone),
      failed_(false),
      failure_location_(start),
      stdlib_name_(kTokenNone),
      foreign_name_(kTokenNone),
      heap_name_(kTokenNone),
      inside_heap_assignment_(false),
      heap_access_type_(nullptr),
      block_stack_(zone),
      pending_label_(0),
      global_imports_(zone) {
  InitializeStdlibTypes();
  Handle<String> source(String::cast(script->source()), isolate);
  std::unique_ptr<Utf16CharacterStream> stream(
      ScannerStream::For(source, start, end));
  scanner_.SetStream(std::move(stream));
}

void AsmJsParser::InitializeStdlibTypes() {
  auto* d = AsmType::Double();
  auto* dq = AsmType::DoubleQ();
  stdlib_dq2d_ = AsmType::Function(zone(), d);
  stdlib_dq2d_->AsFunctionType()->AddArgument(dq);

  stdlib_dqdq2d_ = AsmType::Function(zone(), d);
  stdlib_dqdq2d_->AsFunctionType()->AddArgument(dq);
  stdlib_dqdq2d_->AsFunctionType()->AddArgument(dq);

  auto* f = AsmType::Float();
  auto* fq = AsmType::FloatQ();
  stdlib_fq2f_ = AsmType::Function(zone(), f);
  stdlib_fq2f_->AsFunctionType()->AddArgument(fq);

  auto* s = AsmType::Signed();
  auto* s2s = AsmType::Function(zone(), s);
  s2s->AsFunctionType()->AddArgument(s);

  auto* i = AsmType::Int();
  stdlib_i2s_ = AsmType::Function(zone_, s);
  stdlib_i2s_->AsFunctionType()->AddArgument(i);

  stdlib_ii2s_ = AsmType::Function(zone(), s);
  stdlib_ii2s_->AsFunctionType()->AddArgument(i);
  stdlib_ii2s_->AsFunctionType()->AddArgument(i);

  auto* minmax_d = AsmType::MinMaxType(zone(), d, d);
  // *VIOLATION* The float variant is not part of the spec, but firefox accepts
  // it.
  auto* minmax_f = AsmType::MinMaxType(zone(), f, f);
  auto* minmax_i = AsmType::MinMaxType(zone(), s, i);
  stdlib_minmax_ = AsmType::OverloadedFunction(zone());
  stdlib_minmax_->AsOverloadedFunctionType()->AddOverload(minmax_i);
  stdlib_minmax_->AsOverloadedFunctionType()->AddOverload(minmax_f);
  stdlib_minmax_->AsOverloadedFunctionType()->AddOverload(minmax_d);

  stdlib_abs_ = AsmType::OverloadedFunction(zone());
  stdlib_abs_->AsOverloadedFunctionType()->AddOverload(s2s);
  stdlib_abs_->AsOverloadedFunctionType()->AddOverload(stdlib_dq2d_);
  stdlib_abs_->AsOverloadedFunctionType()->AddOverload(stdlib_fq2f_);

  stdlib_ceil_like_ = AsmType::OverloadedFunction(zone());
  stdlib_ceil_like_->AsOverloadedFunctionType()->AddOverload(stdlib_dq2d_);
  stdlib_ceil_like_->AsOverloadedFunctionType()->AddOverload(stdlib_fq2f_);

  stdlib_fround_ = AsmType::FroundType(zone());
}

FunctionSig* AsmJsParser::ConvertSignature(
    AsmType* return_type, const std::vector<AsmType*>& params) {
  FunctionSig::Builder sig_builder(
      zone(), !return_type->IsA(AsmType::Void()) ? 1 : 0, params.size());
  for (auto param : params) {
    if (param->IsA(AsmType::Double())) {
      sig_builder.AddParam(kWasmF64);
    } else if (param->IsA(AsmType::Float())) {
      sig_builder.AddParam(kWasmF32);
    } else if (param->IsA(AsmType::Int())) {
      sig_builder.AddParam(kWasmI32);
    } else {
      return nullptr;
    }
  }
  if (!return_type->IsA(AsmType::Void())) {
    if (return_type->IsA(AsmType::Double())) {
      sig_builder.AddReturn(kWasmF64);
    } else if (return_type->IsA(AsmType::Float())) {
      sig_builder.AddReturn(kWasmF32);
    } else if (return_type->IsA(AsmType::Signed())) {
      sig_builder.AddReturn(kWasmI32);
    } else {
      return 0;
    }
  }
  return sig_builder.Build();
}

bool AsmJsParser::Run() {
  ValidateModule();
  return !failed_;
}

class AsmJsParser::TemporaryVariableScope {
 public:
  explicit TemporaryVariableScope(AsmJsParser* parser) : parser_(parser) {
    local_depth_ = parser_->function_temp_locals_depth_;
    parser_->function_temp_locals_depth_++;
  }
  ~TemporaryVariableScope() {
    DCHECK_EQ(local_depth_, parser_->function_temp_locals_depth_ - 1);
    parser_->function_temp_locals_depth_--;
  }
  uint32_t get() const { return parser_->TempVariable(local_depth_); }

 private:
  AsmJsParser* parser_;
  int local_depth_;
};

AsmJsParser::VarInfo::VarInfo()
    : type(AsmType::None()),
      function_builder(nullptr),
      import(nullptr),
      mask(-1),
      index(0),
      kind(VarKind::kUnused),
      mutable_variable(true),
      function_defined(false) {}

wasm::AsmJsParser::VarInfo* AsmJsParser::GetVarInfo(
    AsmJsScanner::token_t token) {
  if (AsmJsScanner::IsGlobal(token)) {
    size_t old = global_var_info_.size();
    size_t index = AsmJsScanner::GlobalIndex(token);
    size_t sz = std::max(old, index + 1);
    if (sz != old) {
      global_var_info_.resize(sz);
    }
    return &global_var_info_[index];
  } else if (AsmJsScanner::IsLocal(token)) {
    size_t old = local_var_info_.size();
    size_t index = AsmJsScanner::LocalIndex(token);
    size_t sz = std::max(old, index + 1);
    if (sz != old) {
      local_var_info_.resize(sz);
    }
    return &local_var_info_[index];
  }
  UNREACHABLE();
  return nullptr;
}

uint32_t AsmJsParser::VarIndex(VarInfo* info) {
  if (info->import != nullptr) {
    return info->index;
  } else {
    return info->index + static_cast<uint32_t>(global_imports_.size());
  }
}

void AsmJsParser::AddGlobalImport(std::string name, AsmType* type,
                                  ValueType vtype, bool mutable_variable,
                                  VarInfo* info) {
  // TODO(bradnelson): Refactor memory management here.
  // AsmModuleBuilder should really own import names.
  char* name_data = zone()->NewArray<char>(name.size());
  memcpy(name_data, name.data(), name.size());
  if (mutable_variable) {
    // Allocate a separate variable for the import.
    DeclareGlobal(info, true, type, vtype);
    // Record the need to initialize the global from the import.
    global_imports_.push_back({name_data, name.size(), 0, info->index, true});
  } else {
    // Just use the import directly.
    global_imports_.push_back({name_data, name.size(), 0, info->index, false});
  }
  GlobalImport& gi = global_imports_.back();
  // TODO(bradnelson): Reuse parse buffer memory / make wasm-module-builder
  // managed the memory for the import name (currently have to keep our
  // own memory for it).
  gi.import_index = module_builder_->AddGlobalImport(
      name_data, static_cast<int>(name.size()), vtype);
  if (!mutable_variable) {
    info->DeclareGlobalImport(type, gi.import_index);
  }
}

void AsmJsParser::VarInfo::DeclareGlobalImport(AsmType* type, uint32_t index) {
  kind = VarKind::kGlobal;
  this->type = type;
  this->index = index;
  mutable_variable = false;
}

void AsmJsParser::VarInfo::DeclareStdlibFunc(VarKind kind, AsmType* type) {
  this->kind = kind;
  this->type = type;
  index = 0;  // unused
  mutable_variable = false;
}

void AsmJsParser::DeclareGlobal(VarInfo* info, bool mutable_variable,
                                AsmType* type, ValueType vtype,
                                const WasmInitExpr& init) {
  info->kind = VarKind::kGlobal;
  info->type = type;
  info->index = module_builder_->AddGlobal(vtype, false, true, init);
  info->mutable_variable = mutable_variable;
}

uint32_t AsmJsParser::TempVariable(int index) {
  if (index + 1 > function_temp_locals_used_) {
    function_temp_locals_used_ = index + 1;
  }
  return function_temp_locals_offset_ + index;
}

void AsmJsParser::SkipSemicolon() {
  if (Check(';')) {
    // Had a semicolon.
  } else if (!Peek('}') && !scanner_.IsPrecededByNewline()) {
    FAIL("Expected ;");
  }
}

void AsmJsParser::Begin(AsmJsScanner::token_t label) {
  BareBegin(BlockKind::kRegular, label);
  current_function_builder_->EmitWithU8(kExprBlock, kLocalVoid);
}

void AsmJsParser::Loop(AsmJsScanner::token_t label) {
  BareBegin(BlockKind::kLoop, label);
  current_function_builder_->EmitWithU8(kExprLoop, kLocalVoid);
}

void AsmJsParser::End() {
  BareEnd();
  current_function_builder_->Emit(kExprEnd);
}

void AsmJsParser::BareBegin(BlockKind kind, AsmJsScanner::token_t label) {
  BlockInfo info;
  info.kind = kind;
  info.label = label;
  block_stack_.push_back(info);
}

void AsmJsParser::BareEnd() {
  DCHECK(block_stack_.size() > 0);
  block_stack_.pop_back();
}

int AsmJsParser::FindContinueLabelDepth(AsmJsScanner::token_t label) {
  int count = 0;
  for (auto it = block_stack_.rbegin(); it != block_stack_.rend();
       ++it, ++count) {
    if (it->kind == BlockKind::kLoop &&
        (label == kTokenNone || it->label == label)) {
      return count;
    }
  }
  return -1;
}

int AsmJsParser::FindBreakLabelDepth(AsmJsScanner::token_t label) {
  int count = 0;
  for (auto it = block_stack_.rbegin(); it != block_stack_.rend();
       ++it, ++count) {
    if (it->kind == BlockKind::kRegular &&
        (label == kTokenNone || it->label == label)) {
      return count;
    }
  }
  return -1;
}

// 6.1 ValidateModule
void AsmJsParser::ValidateModule() {
  RECURSE(ValidateModuleParameters());
  EXPECT_TOKEN('{');
  EXPECT_TOKEN(TOK(UseAsm));
  SkipSemicolon();
  RECURSE(ValidateModuleVars());
  while (Peek(TOK(function))) {
    RECURSE(ValidateFunction());
  }
  while (Peek(TOK(var))) {
    RECURSE(ValidateFunctionTable());
  }
  RECURSE(ValidateExport());

  // Check that all functions were eventually defined.
  for (auto& info : global_var_info_) {
    if (info.kind == VarKind::kFunction && !info.function_defined) {
      FAIL("Undefined function");
    }
    if (info.kind == VarKind::kTable && !info.function_defined) {
      FAIL("Undefined function table");
    }
  }

  // Add start function to init things.
  WasmFunctionBuilder* start = module_builder_->AddFunction();
  module_builder_->MarkStartFunction(start);
  for (auto global_import : global_imports_) {
    if (global_import.needs_init) {
      start->EmitWithVarInt(kExprGetGlobal, global_import.import_index);
      start->EmitWithVarInt(kExprSetGlobal,
                            static_cast<uint32_t>(global_import.global_index +
                                                  global_imports_.size()));
    }
  }
  start->Emit(kExprEnd);
  FunctionSig::Builder b(zone(), 0, 0);
  start->SetSignature(b.Build());
}

// 6.1 ValidateModule - parameters
void AsmJsParser::ValidateModuleParameters() {
  EXPECT_TOKEN('(');
  stdlib_name_ = 0;
  foreign_name_ = 0;
  heap_name_ = 0;
  if (!Peek(')')) {
    if (!scanner_.IsGlobal()) {
      FAIL("Expected stdlib parameter");
    }
    stdlib_name_ = Consume();
    if (!Peek(')')) {
      EXPECT_TOKEN(',');
      if (!scanner_.IsGlobal()) {
        FAIL("Expected foreign parameter");
      }
      foreign_name_ = Consume();
      if (!Peek(')')) {
        EXPECT_TOKEN(',');
        if (!scanner_.IsGlobal()) {
          FAIL("Expected heap parameter");
        }
        heap_name_ = Consume();
      }
    }
  }
  EXPECT_TOKEN(')');
}

// 6.1 ValidateModule - variables
void AsmJsParser::ValidateModuleVars() {
  while (Peek(TOK(var)) || Peek(TOK(const))) {
    bool mutable_variable = true;
    if (Check(TOK(var))) {
      // Had a var.
    } else {
      EXPECT_TOKEN(TOK(const));
      mutable_variable = false;
    }
    for (;;) {
      RECURSE(ValidateModuleVar(mutable_variable));
      if (Check(',')) {
        continue;
      }
      break;
    }
    SkipSemicolon();
  }
}

// 6.1 ValidateModule - one variable
void AsmJsParser::ValidateModuleVar(bool mutable_variable) {
  if (!scanner_.IsGlobal()) {
    FAIL("Expected identifier");
  }
  VarInfo* info = GetVarInfo(Consume());
  if (info->kind != VarKind::kUnused) {
    FAIL("Redefinition of variable");
  }
  EXPECT_TOKEN('=');
  double dvalue = 0.0;
  uint64_t uvalue = 0;
  if (CheckForDouble(&dvalue)) {
    DeclareGlobal(info, mutable_variable, AsmType::Double(), kWasmF64,
                  WasmInitExpr(dvalue));
  } else if (CheckForUnsigned(&uvalue)) {
    if (uvalue > 0x7fffffff) {
      FAIL("Numeric literal out of range");
    }
    DeclareGlobal(info, mutable_variable,
                  mutable_variable ? AsmType::Int() : AsmType::Signed(),
                  kWasmI32, WasmInitExpr(static_cast<int32_t>(uvalue)));
  } else if (Check('-')) {
    if (CheckForDouble(&dvalue)) {
      DeclareGlobal(info, mutable_variable, AsmType::Double(), kWasmF64,
                    WasmInitExpr(-dvalue));
    } else if (CheckForUnsigned(&uvalue)) {
      if (uvalue > 0x7fffffff) {
        FAIL("Numeric literal out of range");
      }
      DeclareGlobal(info, mutable_variable,
                    mutable_variable ? AsmType::Int() : AsmType::Signed(),
                    kWasmI32, WasmInitExpr(-static_cast<int32_t>(uvalue)));
    } else {
      FAIL("Expected numeric literal");
    }
  } else if (Check(TOK(new))) {
    RECURSE(ValidateModuleVarNewStdlib(info));
  } else if (Check(stdlib_name_)) {
    EXPECT_TOKEN('.');
    RECURSE(ValidateModuleVarStdlib(info));
  } else if (ValidateModuleVarImport(info, mutable_variable)) {
    // Handled inside.
  } else if (scanner_.IsGlobal()) {
    RECURSE(ValidateModuleVarFromGlobal(info, mutable_variable));
  } else {
    FAIL("Bad variable declaration");
  }
}

// 6.1 ValidateModule - global float declaration
void AsmJsParser::ValidateModuleVarFromGlobal(VarInfo* info,
                                              bool mutable_variable) {
  VarInfo* src_info = GetVarInfo(Consume());
  if (!src_info->type->IsA(stdlib_fround_)) {
    if (src_info->mutable_variable) {
      FAIL("Can only use immutable variables in global definition");
    }
    if (mutable_variable) {
      FAIL("Can only define immutable variables with other immutables");
    }
    if (!src_info->type->IsA(AsmType::Int()) &&
        !src_info->type->IsA(AsmType::Float()) &&
        !src_info->type->IsA(AsmType::Double())) {
      FAIL("Expected int, float, double, or fround for global definition");
    }
    info->kind = VarKind::kGlobal;
    info->type = src_info->type;
    info->index = src_info->index;
    info->mutable_variable = false;
    return;
  }
  EXPECT_TOKEN('(');
  bool negate = false;
  if (Check('-')) {
    negate = true;
  }
  double dvalue = 0.0;
  uint64_t uvalue = 0;
  if (CheckForDouble(&dvalue)) {
    if (negate) {
      dvalue = -dvalue;
    }
    DeclareGlobal(info, mutable_variable, AsmType::Float(), kWasmF32,
                  WasmInitExpr(static_cast<float>(dvalue)));
  } else if (CheckForUnsigned(&uvalue)) {
    dvalue = uvalue;
    if (negate) {
      dvalue = -dvalue;
    }
    DeclareGlobal(info, mutable_variable, AsmType::Float(), kWasmF32,
                  WasmInitExpr(static_cast<float>(dvalue)));
  } else {
    FAIL("Expected numeric literal");
  }
  EXPECT_TOKEN(')');
}

// 6.1 ValidateModule - foreign imports
bool AsmJsParser::ValidateModuleVarImport(VarInfo* info,
                                          bool mutable_variable) {
  if (Check('+')) {
    EXPECT_TOKENf(foreign_name_);
    EXPECT_TOKENf('.');
    AddGlobalImport(scanner_.GetIdentifierString(), AsmType::Double(), kWasmF64,
                    mutable_variable, info);
    scanner_.Next();
    return true;
  } else if (Check(foreign_name_)) {
    EXPECT_TOKENf('.');
    std::string import_name = scanner_.GetIdentifierString();
    scanner_.Next();
    if (Check('|')) {
      if (!CheckForZero()) {
        FAILf("Expected |0 type annotation for foreign integer import");
      }
      AddGlobalImport(import_name, AsmType::Int(), kWasmI32, mutable_variable,
                      info);
      return true;
    }
    info->kind = VarKind::kImportedFunction;
    function_import_info_.resize(function_import_info_.size() + 1);
    info->import = &function_import_info_.back();
    // TODO(bradnelson): Refactor memory management here.
    // AsmModuleBuilder should really own import names.
    info->import->function_name = zone()->NewArray<char>(import_name.size());
    memcpy(info->import->function_name, import_name.data(), import_name.size());
    info->import->function_name_size = import_name.size();
    return true;
  }
  return false;
}

// 6.1 ValidateModule - one variable
// 9 - Standard Library - heap types
void AsmJsParser::ValidateModuleVarNewStdlib(VarInfo* info) {
  EXPECT_TOKEN(stdlib_name_);
  EXPECT_TOKEN('.');
  switch (Consume()) {
#define V(name, _junk1, _junk2, _junk3)                          \
  case TOK(name):                                                \
    info->DeclareStdlibFunc(VarKind::kSpecial, AsmType::name()); \
    break;
    STDLIB_ARRAY_TYPE_LIST(V)
#undef V
    default:
      FAIL("Expected ArrayBuffer view");
      break;
  }
  EXPECT_TOKEN('(');
  EXPECT_TOKEN(heap_name_);
  EXPECT_TOKEN(')');
}

// 6.1 ValidateModule - one variable
// 9 - Standard Library
void AsmJsParser::ValidateModuleVarStdlib(VarInfo* info) {
  if (Check(TOK(Math))) {
    EXPECT_TOKEN('.');
    switch (Consume()) {
#define V(name)                                             \
  case TOK(name):                                           \
    DeclareGlobal(info, false, AsmType::Double(), kWasmF64, \
                  WasmInitExpr(M_##name));                  \
    stdlib_uses_.insert(AsmTyper::kMath##name);             \
    break;
      STDLIB_MATH_VALUE_LIST(V)
#undef V
#define V(name, Name, op, sig)                                      \
  case TOK(name):                                                   \
    info->DeclareStdlibFunc(VarKind::kMath##Name, stdlib_##sig##_); \
    stdlib_uses_.insert(AsmTyper::kMath##Name);                     \
    break;
      STDLIB_MATH_FUNCTION_LIST(V)
#undef V
      default:
        FAIL("Invalid member of stdlib.Math");
    }
  } else if (Check(TOK(Infinity))) {
    DeclareGlobal(info, false, AsmType::Double(), kWasmF64,
                  WasmInitExpr(std::numeric_limits<double>::infinity()));
    stdlib_uses_.insert(AsmTyper::kInfinity);
  } else if (Check(TOK(NaN))) {
    DeclareGlobal(info, false, AsmType::Double(), kWasmF64,
                  WasmInitExpr(std::numeric_limits<double>::quiet_NaN()));
    stdlib_uses_.insert(AsmTyper::kNaN);
  } else {
    FAIL("Invalid member of stdlib");
  }
}

// 6.2 ValidateExport
void AsmJsParser::ValidateExport() {
  // clang-format off
  EXPECT_TOKEN(TOK(return));
  // clang format on
  if (Check('{')) {
    for (;;) {
      std::string name = scanner_.GetIdentifierString();
      if (!scanner_.IsGlobal() && !scanner_.IsLocal()) {
        FAIL("Illegal export name");
      }
      Consume();
      EXPECT_TOKEN(':');
      if (!scanner_.IsGlobal()) {
        FAIL("Expected function name");
      }
      VarInfo* info = GetVarInfo(Consume());
      if (info->kind != VarKind::kFunction) {
        FAIL("Expected function");
      }
      info->function_builder->ExportAs(
          {name.c_str(), static_cast<int>(name.size())});
      if (Check(',')) {
        if (!Peek('}')) {
          continue;
        }
      }
      break;
    }
    EXPECT_TOKEN('}');
  } else {
    if (!scanner_.IsGlobal()) {
      FAIL("Single function export must be a function name");
    }
    VarInfo* info = GetVarInfo(Consume());
    if (info->kind != VarKind::kFunction) {
      FAIL("Single function export must be a function");
    }
    const char* single_function_name = "__single_function__";
    info->function_builder->ExportAs(CStrVector(single_function_name));
  }
}

// 6.3 ValidateFunctionTable
void AsmJsParser::ValidateFunctionTable() {
  EXPECT_TOKEN(TOK(var));
  if (!scanner_.IsGlobal()) {
    FAIL("Expected table name");
  }
  VarInfo* table_info = GetVarInfo(Consume());
  if (table_info->kind == VarKind::kTable) {
    if (table_info->function_defined) {
      FAIL("Function table redefined");
    }
    table_info->function_defined = true;
  } else if (table_info->kind != VarKind::kUnused) {
    FAIL("Function table name collides");
  }
  EXPECT_TOKEN('=');
  EXPECT_TOKEN('[');
  uint64_t count = 0;
  for (;;) {
    if (!scanner_.IsGlobal()) {
      FAIL("Expected function name");
    }
    VarInfo* info = GetVarInfo(Consume());
    if (info->kind != VarKind::kFunction) {
      FAIL("Expected function");
    }
    // Only store the function into a table if we used the table somewhere
    // (i.e. tables are first seen at their use sites and allocated there).
    if (table_info->kind == VarKind::kTable) {
      DCHECK_GE(table_info->mask, 0);
      if (count >= static_cast<uint64_t>(table_info->mask) + 1) {
        FAIL("Exceeded function table size");
      }
      if (!info->type->IsA(table_info->type)) {
        FAIL("Function table definition doesn't match use");
      }
      module_builder_->SetIndirectFunction(
          static_cast<uint32_t>(table_info->index + count), info->index);
    }
    ++count;
    if (Check(',')) {
      if (!Peek(']')) {
        continue;
      }
    }
    break;
  }
  EXPECT_TOKEN(']');
  if (table_info->kind == VarKind::kTable &&
      count != static_cast<uint64_t>(table_info->mask) + 1) {
    FAIL("Function table size does not match uses");
  }
  SkipSemicolon();
}

// 6.4 ValidateFunction
void AsmJsParser::ValidateFunction() {
  EXPECT_TOKEN(TOK(function));
  if (!scanner_.IsGlobal()) {
    FAIL("Expected function name");
  }

  std::string function_name_raw = scanner_.GetIdentifierString();
  AsmJsScanner::token_t function_name = Consume();
  VarInfo* function_info = GetVarInfo(function_name);
  if (function_info->kind == VarKind::kUnused) {
    function_info->kind = VarKind::kFunction;
    function_info->function_builder = module_builder_->AddFunction();
    function_info->index = function_info->function_builder->func_index();
  } else if (function_info->kind != VarKind::kFunction) {
    FAIL("Function name collides with variable");
  } else if (function_info->function_defined) {
    FAIL("Function redefined");
  }

  function_info->function_defined = true;
  // TODO(bradnelson): Cleanup memory management here.
  // WasmModuleBuilder should own these.
  char* function_name_chr = zone()->NewArray<char>(function_name_raw.size());
  memcpy(function_name_chr, function_name_raw.data(), function_name_raw.size());
  function_info->function_builder->SetName(
      {function_name_chr, static_cast<int>(function_name_raw.size())});
  current_function_builder_ = function_info->function_builder;
  return_type_ = nullptr;

  // Record start of the function, used as position for the stack check.
  int start_position = static_cast<int>(scanner_.Position());
  current_function_builder_->SetAsmFunctionStartPosition(start_position);

  std::vector<AsmType*> params;
  ValidateFunctionParams(&params);
  std::vector<ValueType> locals;
  ValidateFunctionLocals(params.size(), &locals);

  function_temp_locals_offset_ = static_cast<uint32_t>(
      params.size() + locals.size());
  function_temp_locals_used_ = 0;
  function_temp_locals_depth_ = 0;

  while (!failed_ && !Peek('}')) {
    RECURSE(ValidateStatement());
  }
  EXPECT_TOKEN('}');

  if (return_type_ == nullptr) {
    return_type_ = AsmType::Void();
  }

  // TODO(bradnelson): WasmModuleBuilder can't take this in the right order.
  //                   We should fix that so we can use it instead.
  FunctionSig* sig = ConvertSignature(return_type_, params);
  if (sig == nullptr) {
    FAIL("Invalid function signature in declaration");
  }
  current_function_builder_->SetSignature(sig);
  for (auto local : locals) {
    current_function_builder_->AddLocal(local);
  }
  // Add bonus temps.
  for (int i = 0; i < function_temp_locals_used_; ++i) {
    current_function_builder_->AddLocal(kWasmI32);
  }

  // End function
  current_function_builder_->Emit(kExprEnd);

  // Record (or validate) function type.
  AsmType* function_type = AsmType::Function(zone(), return_type_);
  for (auto t : params) {
    function_type->AsFunctionType()->AddArgument(t);
  }
  function_info = GetVarInfo(function_name);
  if (function_info->type->IsA(AsmType::None())) {
    DCHECK(function_info->kind == VarKind::kFunction);
    function_info->type = function_type;
  } else if (!function_type->IsA(function_info->type)) {
    // TODO(bradnelson): Should IsExactly be used here?
    FAIL("Function definition doesn't match use");
  }

  scanner_.ResetLocals();
  local_var_info_.clear();
}

// 6.4 ValidateFunction
void AsmJsParser::ValidateFunctionParams(std::vector<AsmType*>* params) {
  // TODO(bradnelson): Do this differently so that the scanner doesn't need to
  // have a state transition that needs knowledge of how the scanner works
  // inside.
  scanner_.EnterLocalScope();
  EXPECT_TOKEN('(');
  std::vector<AsmJsScanner::token_t> function_parameters;
  while (!failed_ && !Peek(')')) {
    if (!scanner_.IsLocal()) {
      FAIL("Expected parameter name");
    }
    function_parameters.push_back(Consume());
    if (!Peek(')')) {
      EXPECT_TOKEN(',');
    }
  }
  EXPECT_TOKEN(')');
  scanner_.EnterGlobalScope();
  EXPECT_TOKEN('{');
  // 5.1 Parameter Type Annotations
  for (auto p : function_parameters) {
    EXPECT_TOKEN(p);
    EXPECT_TOKEN('=');
    VarInfo* info = GetVarInfo(p);
    if (info->kind != VarKind::kUnused) {
      FAIL("Duplicate parameter name");
    }
    if (Check(p)) {
      EXPECT_TOKEN('|');
      if (!CheckForZero()) {
        FAIL("Bad integer parameter annotation.");
      }
      info->kind = VarKind::kLocal;
      info->type = AsmType::Int();
      info->index = static_cast<uint32_t>(params->size());
      params->push_back(AsmType::Int());
    } else if (Check('+')) {
      EXPECT_TOKEN(p);
      info->kind = VarKind::kLocal;
      info->type = AsmType::Double();
      info->index = static_cast<uint32_t>(params->size());
      params->push_back(AsmType::Double());
    } else {
      if (!GetVarInfo(Consume())->type->IsA(stdlib_fround_)) {
        FAIL("Expected fround");
      }
      EXPECT_TOKEN('(');
      EXPECT_TOKEN(p);
      EXPECT_TOKEN(')');
      info->kind = VarKind::kLocal;
      info->type = AsmType::Float();
      info->index = static_cast<uint32_t>(params->size());
      params->push_back(AsmType::Float());
    }
    SkipSemicolon();
  }
}

// 6.4 ValidateFunction - locals
void AsmJsParser::ValidateFunctionLocals(
    size_t param_count, std::vector<ValueType>* locals) {
  // Local Variables.
  while (Peek(TOK(var))) {
    scanner_.EnterLocalScope();
    EXPECT_TOKEN(TOK(var));
    scanner_.EnterGlobalScope();
    for (;;) {
      if (!scanner_.IsLocal()) {
        FAIL("Expected local variable identifier");
      }
      VarInfo* info = GetVarInfo(Consume());
      if (info->kind != VarKind::kUnused) {
        FAIL("Duplicate local variable name");
      }
      // Store types.
      EXPECT_TOKEN('=');
      double dvalue = 0.0;
      uint64_t uvalue = 0;
      if (Check('-')) {
        if (CheckForDouble(&dvalue)) {
          info->kind = VarKind::kLocal;
          info->type = AsmType::Double();
          info->index = static_cast<uint32_t>(param_count + locals->size());
          locals->push_back(kWasmF64);
          byte code[] = {WASM_F64(-dvalue)};
          current_function_builder_->EmitCode(code, sizeof(code));
          current_function_builder_->EmitSetLocal(info->index);
        } else if (CheckForUnsigned(&uvalue)) {
          if (uvalue > 0x7fffffff) {
            FAIL("Numeric literal out of range");
          }
          info->kind = VarKind::kLocal;
          info->type = AsmType::Int();
          info->index = static_cast<uint32_t>(param_count + locals->size());
          locals->push_back(kWasmI32);
          int32_t value = -static_cast<int32_t>(uvalue);
          current_function_builder_->EmitI32Const(value);
          current_function_builder_->EmitSetLocal(info->index);
        } else {
          FAIL("Expected variable initial value");
        }
      } else if (scanner_.IsGlobal()) {
        VarInfo* sinfo = GetVarInfo(Consume());
        if (sinfo->kind == VarKind::kGlobal) {
          if (sinfo->mutable_variable) {
            FAIL("Initializing from global requires const variable");
          }
          info->kind = VarKind::kLocal;
          info->type = sinfo->type;
          info->index = static_cast<uint32_t>(param_count + locals->size());
          if (sinfo->type->IsA(AsmType::Int())) {
            locals->push_back(kWasmI32);
          } else if (sinfo->type->IsA(AsmType::Float())) {
            locals->push_back(kWasmF32);
          } else if (sinfo->type->IsA(AsmType::Double())) {
            locals->push_back(kWasmF64);
          } else {
            FAIL("Bad local variable definition");
          }
          current_function_builder_->EmitWithVarInt(kExprGetGlobal,
                                                    VarIndex(sinfo));
          current_function_builder_->EmitSetLocal(info->index);
        } else if (sinfo->type->IsA(stdlib_fround_)) {
          EXPECT_TOKEN('(');
          bool negate = false;
          if (Check('-')) {
            negate = true;
          }
          double dvalue = 0.0;
          if (CheckForDouble(&dvalue)) {
            info->kind = VarKind::kLocal;
            info->type = AsmType::Float();
            info->index = static_cast<uint32_t>(param_count + locals->size());
            locals->push_back(kWasmF32);
            if (negate) {
              dvalue = -dvalue;
            }
            byte code[] = {WASM_F32(dvalue)};
            current_function_builder_->EmitCode(code, sizeof(code));
            current_function_builder_->EmitSetLocal(info->index);
          } else if (CheckForUnsigned(&uvalue)) {
            if (uvalue > 0x7fffffff) {
              FAIL("Numeric literal out of range");
            }
            info->kind = VarKind::kLocal;
            info->type = AsmType::Float();
            info->index = static_cast<uint32_t>(param_count + locals->size());
            locals->push_back(kWasmF32);
            int32_t value = static_cast<int32_t>(uvalue);
            if (negate) {
              value = -value;
            }
            double fvalue = static_cast<double>(value);
            byte code[] = {WASM_F32(fvalue)};
            current_function_builder_->EmitCode(code, sizeof(code));
            current_function_builder_->EmitSetLocal(info->index);
          } else {
            FAIL("Expected variable initial value");
          }
          EXPECT_TOKEN(')');
        } else {
          FAIL("expected fround or const global");
        }
      } else if (CheckForDouble(&dvalue)) {
        info->kind = VarKind::kLocal;
        info->type = AsmType::Double();
        info->index = static_cast<uint32_t>(param_count + locals->size());
        locals->push_back(kWasmF64);
        byte code[] = {WASM_F64(dvalue)};
        current_function_builder_->EmitCode(code, sizeof(code));
        current_function_builder_->EmitSetLocal(info->index);
      } else if (CheckForUnsigned(&uvalue)) {
        info->kind = VarKind::kLocal;
        info->type = AsmType::Int();
        info->index = static_cast<uint32_t>(param_count + locals->size());
        locals->push_back(kWasmI32);
        int32_t value = static_cast<int32_t>(uvalue);
        current_function_builder_->EmitI32Const(value);
        current_function_builder_->EmitSetLocal(info->index);
      } else {
        FAIL("Expected variable initial value");
      }
      if (!Peek(',')) {
        break;
      }
      scanner_.EnterLocalScope();
      EXPECT_TOKEN(',');
      scanner_.EnterGlobalScope();
    }
    SkipSemicolon();
  }
}

// ValidateStatement
void AsmJsParser::ValidateStatement() {
  call_coercion_ = nullptr;
  if (Peek('{')) {
    RECURSE(Block());
  } else if (Peek(';')) {
    RECURSE(EmptyStatement());
  } else if (Peek(TOK(if))) {
    RECURSE(IfStatement());
    // clang-format off
  } else if (Peek(TOK(return))) {
    // clang-format on
    RECURSE(ReturnStatement());
  } else if (IterationStatement()) {
    // Handled in IterationStatement.
  } else if (Peek(TOK(break))) {
    RECURSE(BreakStatement());
  } else if (Peek(TOK(continue))) {
    RECURSE(ContinueStatement());
  } else if (Peek(TOK(switch))) {
    RECURSE(SwitchStatement());
  } else {
    RECURSE(ExpressionStatement());
  }
}

// 6.5.1 Block
void AsmJsParser::Block() {
  bool can_break_to_block = pending_label_ != 0;
  if (can_break_to_block) {
    Begin(pending_label_);
  }
  pending_label_ = 0;
  EXPECT_TOKEN('{');
  while (!failed_ && !Peek('}')) {
    RECURSE(ValidateStatement());
  }
  EXPECT_TOKEN('}');
  if (can_break_to_block) {
    End();
  }
}

// 6.5.2 ExpressionStatement
void AsmJsParser::ExpressionStatement() {
  if (scanner_.IsGlobal() || scanner_.IsLocal()) {
    // NOTE: Both global or local identifiers can also be used as labels.
    scanner_.Next();
    if (Peek(':')) {
      scanner_.Rewind();
      RECURSE(LabelledStatement());
      return;
    }
    scanner_.Rewind();
  }
  AsmType* ret;
  RECURSE(ret = ValidateExpression());
  if (!ret->IsA(AsmType::Void())) {
    current_function_builder_->Emit(kExprDrop);
  }
  SkipSemicolon();
}

// 6.5.3 EmptyStatement
void AsmJsParser::EmptyStatement() { EXPECT_TOKEN(';'); }

// 6.5.4 IfStatement
void AsmJsParser::IfStatement() {
  EXPECT_TOKEN(TOK(if));
  EXPECT_TOKEN('(');
  RECURSE(Expression(AsmType::Int()));
  EXPECT_TOKEN(')');
  current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
  BareBegin();
  RECURSE(ValidateStatement());
  if (Check(TOK(else))) {
    current_function_builder_->Emit(kExprElse);
    RECURSE(ValidateStatement());
  }
  current_function_builder_->Emit(kExprEnd);
  BareEnd();
}

// 6.5.5 ReturnStatement
void AsmJsParser::ReturnStatement() {
  // clang-format off
  EXPECT_TOKEN(TOK(return ));
  // clang-format on
  if (!Peek(';') && !Peek('}')) {
    // TODO(bradnelson): See if this can be factored out.
    AsmType* ret;
    RECURSE(ret = Expression(return_type_));
    if (ret->IsA(AsmType::Double())) {
      return_type_ = AsmType::Double();
    } else if (ret->IsA(AsmType::Float())) {
      return_type_ = AsmType::Float();
    } else if (ret->IsA(AsmType::Signed())) {
      return_type_ = AsmType::Signed();
    } else {
      FAIL("Invalid return type");
    }
  } else {
    return_type_ = AsmType::Void();
  }
  current_function_builder_->Emit(kExprReturn);
  SkipSemicolon();
}

// 6.5.6 IterationStatement
bool AsmJsParser::IterationStatement() {
  if (Peek(TOK(while))) {
    WhileStatement();
  } else if (Peek(TOK(do))) {
    DoStatement();
  } else if (Peek(TOK(for))) {
    ForStatement();
  } else {
    return false;
  }
  return true;
}

// 6.5.6 IterationStatement - while
void AsmJsParser::WhileStatement() {
  // a: block {
  Begin(pending_label_);
  //   b: loop {
  Loop(pending_label_);
  pending_label_ = 0;
  EXPECT_TOKEN(TOK(while));
  EXPECT_TOKEN('(');
  RECURSE(Expression(AsmType::Int()));
  EXPECT_TOKEN(')');
  //     if (!CONDITION) break a;
  current_function_builder_->Emit(kExprI32Eqz);
  current_function_builder_->EmitWithU8(kExprBrIf, 1);
  //     BODY
  RECURSE(ValidateStatement());
  //     continue b;
  current_function_builder_->EmitWithU8(kExprBr, 0);
  End();
  //   }
  // }
  End();
}

// 6.5.6 IterationStatement - do
void AsmJsParser::DoStatement() {
  // a: block {
  Begin(pending_label_);
  //   b: loop {
  Loop();
  //     c: block {  // but treated like loop so continue works
  BareBegin(BlockKind::kLoop, pending_label_);
  current_function_builder_->EmitWithU8(kExprBlock, kLocalVoid);
  pending_label_ = 0;
  EXPECT_TOKEN(TOK(do));
  //       BODY
  RECURSE(ValidateStatement());
  EXPECT_TOKEN(TOK(while));
  End();
  //     }
  EXPECT_TOKEN('(');
  RECURSE(Expression(AsmType::Int()));
  //     if (CONDITION) break a;
  current_function_builder_->Emit(kExprI32Eqz);
  current_function_builder_->EmitWithU8(kExprBrIf, 1);
  //     continue b;
  current_function_builder_->EmitWithU8(kExprBr, 0);
  EXPECT_TOKEN(')');
  //   }
  End();
  // }
  End();
  SkipSemicolon();
}

// 6.5.6 IterationStatement - for
void AsmJsParser::ForStatement() {
  EXPECT_TOKEN(TOK(for));
  EXPECT_TOKEN('(');
  if (!Peek(';')) {
    Expression(nullptr);
  }
  EXPECT_TOKEN(';');
  // a: block {
  Begin(pending_label_);
  //   b: loop {
  Loop(pending_label_);
  pending_label_ = 0;
  if (!Peek(';')) {
    // if (CONDITION) break a;
    RECURSE(Expression(AsmType::Int()));
    current_function_builder_->Emit(kExprI32Eqz);
    current_function_builder_->EmitWithU8(kExprBrIf, 1);
  }
  EXPECT_TOKEN(';');
  // Stash away INCREMENT
  size_t increment_position = current_function_builder_->GetPosition();
  if (!Peek(')')) {
    RECURSE(Expression(nullptr));
  }
  std::vector<byte> increment_code;
  current_function_builder_->StashCode(&increment_code, increment_position);
  EXPECT_TOKEN(')');
  //       BODY
  RECURSE(ValidateStatement());
  //       INCREMENT
  current_function_builder_->EmitCode(
      increment_code.data(), static_cast<uint32_t>(increment_code.size()));
  current_function_builder_->EmitWithU8(kExprBr, 0);
  //   }
  End();
  // }
  End();
}

// 6.5.7 BreakStatement
void AsmJsParser::BreakStatement() {
  EXPECT_TOKEN(TOK(break));
  AsmJsScanner::token_t label_name = kTokenNone;
  if (scanner_.IsGlobal() || scanner_.IsLocal()) {
    // NOTE: Currently using globals/locals for labels too.
    label_name = Consume();
  }
  int depth = FindBreakLabelDepth(label_name);
  if (depth < 0) {
    FAIL("Illegal break");
  }
  current_function_builder_->Emit(kExprBr);
  current_function_builder_->EmitVarInt(depth);
  SkipSemicolon();
}

// 6.5.8 ContinueStatement
void AsmJsParser::ContinueStatement() {
  EXPECT_TOKEN(TOK(continue));
  AsmJsScanner::token_t label_name = kTokenNone;
  if (scanner_.IsGlobal() || scanner_.IsLocal()) {
    // NOTE: Currently using globals/locals for labels too.
    label_name = Consume();
  }
  int depth = FindContinueLabelDepth(label_name);
  if (depth < 0) {
    FAIL("Illegal continue");
  }
  current_function_builder_->Emit(kExprBr);
  current_function_builder_->EmitVarInt(depth);
  SkipSemicolon();
}

// 6.5.9 LabelledStatement
void AsmJsParser::LabelledStatement() {
  DCHECK(scanner_.IsGlobal() || scanner_.IsLocal());
  // NOTE: Currently using globals/locals for labels too.
  if (pending_label_ != 0) {
    FAIL("Double label unsupported");
  }
  pending_label_ = scanner_.Token();
  scanner_.Next();
  EXPECT_TOKEN(':');
  RECURSE(ValidateStatement());
}

// 6.5.10 SwitchStatement
void AsmJsParser::SwitchStatement() {
  EXPECT_TOKEN(TOK(switch));
  EXPECT_TOKEN('(');
  AsmType* test;
  RECURSE(test = Expression(nullptr));
  if (!test->IsA(AsmType::Signed())) {
    FAIL("Expected signed for switch value");
  }
  EXPECT_TOKEN(')');
  uint32_t tmp = TempVariable(0);
  current_function_builder_->EmitSetLocal(tmp);
  Begin(pending_label_);
  pending_label_ = 0;
  // TODO(bradnelson): Make less weird.
  std::vector<int32_t> cases;
  GatherCases(&cases);  // Skips { implicitly.
  size_t count = cases.size() + 1;
  for (size_t i = 0; i < count; ++i) {
    BareBegin(BlockKind::kOther);
    current_function_builder_->EmitWithU8(kExprBlock, kLocalVoid);
  }
  int table_pos = 0;
  for (auto c : cases) {
    current_function_builder_->EmitGetLocal(tmp);
    current_function_builder_->EmitI32Const(c);
    current_function_builder_->Emit(kExprI32Eq);
    current_function_builder_->EmitWithVarInt(kExprBrIf, table_pos++);
  }
  current_function_builder_->EmitWithVarInt(kExprBr, table_pos++);
  while (!failed_ && Peek(TOK(case))) {
    current_function_builder_->Emit(kExprEnd);
    BareEnd();
    RECURSE(ValidateCase());
  }
  current_function_builder_->Emit(kExprEnd);
  BareEnd();
  if (Peek(TOK(default))) {
    RECURSE(ValidateDefault());
  }
  EXPECT_TOKEN('}');
  End();
}

// 6.6. ValidateCase
void AsmJsParser::ValidateCase() {
  EXPECT_TOKEN(TOK(case));
  bool negate = false;
  if (Check('-')) {
    negate = true;
  }
  uint64_t uvalue;
  if (!CheckForUnsigned(&uvalue)) {
    FAIL("Expected numeric literal");
  }
  // TODO(bradnelson): Share negation plumbing.
  if ((negate && uvalue > 0x80000000) || (!negate && uvalue > 0x7fffffff)) {
    FAIL("Numeric literal out of range");
  }
  int32_t value = static_cast<int32_t>(uvalue);
  if (negate) {
    value = -value;
  }
  EXPECT_TOKEN(':');
  while (!failed_ && !Peek('}') && !Peek(TOK(case)) && !Peek(TOK(default))) {
    RECURSE(ValidateStatement());
  }
}

// 6.7 ValidateDefault
void AsmJsParser::ValidateDefault() {
  EXPECT_TOKEN(TOK(default));
  EXPECT_TOKEN(':');
  while (!failed_ && !Peek('}')) {
    RECURSE(ValidateStatement());
  }
}

// 6.8 ValidateExpression
AsmType* AsmJsParser::ValidateExpression() {
  AsmType* ret;
  RECURSEn(ret = Expression(nullptr));
  return ret;
}

// 6.8.1 Expression
AsmType* AsmJsParser::Expression(AsmType* expected) {
  AsmType* a;
  for (;;) {
    RECURSEn(a = AssignmentExpression());
    if (Peek(',')) {
      if (a->IsA(AsmType::None())) {
        FAILn("Expected actual type");
      }
      if (!a->IsA(AsmType::Void())) {
        current_function_builder_->Emit(kExprDrop);
      }
      EXPECT_TOKENn(',');
      continue;
    }
    break;
  }
  if (expected != nullptr && !a->IsA(expected)) {
    FAILn("Unexpected type");
  }
  return a;
}

// 6.8.2 NumericLiteral
AsmType* AsmJsParser::NumericLiteral() {
  call_coercion_ = nullptr;
  double dvalue = 0.0;
  uint64_t uvalue = 0;
  if (CheckForDouble(&dvalue)) {
    byte code[] = {WASM_F64(dvalue)};
    current_function_builder_->EmitCode(code, sizeof(code));
    return AsmType::Double();
  } else if (CheckForUnsigned(&uvalue)) {
    if (uvalue <= 0x7fffffff) {
      current_function_builder_->EmitI32Const(static_cast<int32_t>(uvalue));
      return AsmType::FixNum();
    } else if (uvalue <= 0xffffffff) {
      current_function_builder_->EmitI32Const(static_cast<int32_t>(uvalue));
      return AsmType::Unsigned();
    } else {
      FAILn("Integer numeric literal out of range.");
    }
  } else {
    FAILn("Expected numeric literal.");
  }
}

// 6.8.3 Identifier
AsmType* AsmJsParser::Identifier() {
  call_coercion_ = nullptr;
  if (scanner_.IsLocal()) {
    VarInfo* info = GetVarInfo(Consume());
    if (info->kind != VarKind::kLocal) {
      FAILn("Undefined local variable");
    }
    current_function_builder_->EmitGetLocal(info->index);
    return info->type;
  } else if (scanner_.IsGlobal()) {
    VarInfo* info = GetVarInfo(Consume());
    if (info->kind != VarKind::kGlobal) {
      FAILn("Undefined global variable");
    }
    current_function_builder_->EmitWithVarInt(kExprGetGlobal, VarIndex(info));
    return info->type;
  }
  UNREACHABLE();
  return nullptr;
}

// 6.8.4 CallExpression
AsmType* AsmJsParser::CallExpression() {
  AsmType* ret;
  if (scanner_.IsGlobal() &&
      GetVarInfo(scanner_.Token())->type->IsA(stdlib_fround_)) {
    ValidateFloatCoercion();
    return AsmType::Float();
  } else if (scanner_.IsGlobal() &&
             GetVarInfo(scanner_.Token())->type->IsA(AsmType::Heap())) {
    RECURSEn(ret = MemberExpression());
  } else if (Peek('(')) {
    RECURSEn(ret = ParenthesizedExpression());
  } else if (PeekCall()) {
    RECURSEn(ret = ValidateCall());
  } else if (scanner_.IsLocal() || scanner_.IsGlobal()) {
    RECURSEn(ret = Identifier());
  } else {
    RECURSEn(ret = NumericLiteral());
  }
  return ret;
}

// 6.8.5 MemberExpression
AsmType* AsmJsParser::MemberExpression() {
  call_coercion_ = nullptr;
  ValidateHeapAccess();
  if (Peek('=')) {
    inside_heap_assignment_ = true;
    return heap_access_type_->StoreType();
  } else {
#define V(array_type, wasmload, wasmstore, type)                       \
  if (heap_access_type_->IsA(AsmType::array_type())) {                 \
    current_function_builder_->Emit(kExpr##type##AsmjsLoad##wasmload); \
    return heap_access_type_->LoadType();                              \
  }
    STDLIB_ARRAY_TYPE_LIST(V)
#undef V
    FAILn("Expected valid heap load");
  }
}

// 6.8.6 AssignmentExpression
AsmType* AsmJsParser::AssignmentExpression() {
  AsmType* ret;
  if (scanner_.IsGlobal() &&
      GetVarInfo(scanner_.Token())->type->IsA(AsmType::Heap())) {
    RECURSEn(ret = ConditionalExpression());
    if (Peek('=')) {
      if (!inside_heap_assignment_) {
        FAILn("Invalid assignment target");
      }
      inside_heap_assignment_ = false;
      AsmType* heap_type = heap_access_type_;
      EXPECT_TOKENn('=');
      AsmType* value;
      RECURSEn(value = AssignmentExpression());
      if (!value->IsA(ret)) {
        FAILn("Illegal type stored to heap view");
      }
      if (heap_type->IsA(AsmType::Float32Array()) &&
          value->IsA(AsmType::Double())) {
        // Assignment to a float32 heap can be used to convert doubles.
        current_function_builder_->Emit(kExprF32ConvertF64);
      }
      ret = value;
#define V(array_type, wasmload, wasmstore, type)                         \
  if (heap_type->IsA(AsmType::array_type())) {                           \
    current_function_builder_->Emit(kExpr##type##AsmjsStore##wasmstore); \
    return ret;                                                          \
  }
      STDLIB_ARRAY_TYPE_LIST(V)
#undef V
    }
  } else if (scanner_.IsLocal() || scanner_.IsGlobal()) {
    bool is_local = scanner_.IsLocal();
    VarInfo* info = GetVarInfo(scanner_.Token());
    USE(is_local);
    ret = info->type;
    scanner_.Next();
    if (Check('=')) {
      // NOTE: Before this point, this might have been VarKind::kUnused even in
      // valid code, as it might be a label.
      if (info->kind == VarKind::kUnused) {
        FAILn("Undeclared assignment target");
      }
      DCHECK(is_local ? info->kind == VarKind::kLocal
                      : info->kind == VarKind::kGlobal);
      AsmType* value;
      RECURSEn(value = AssignmentExpression());
      if (!value->IsA(ret)) {
        FAILn("Type mismatch in assignment");
      }
      if (info->kind == VarKind::kLocal) {
        current_function_builder_->EmitTeeLocal(info->index);
      } else if (info->kind == VarKind::kGlobal) {
        current_function_builder_->EmitWithVarUint(kExprSetGlobal,
                                                   VarIndex(info));
        current_function_builder_->EmitWithVarUint(kExprGetGlobal,
                                                   VarIndex(info));
      } else {
        UNREACHABLE();
      }
      return ret;
    }
    scanner_.Rewind();
    RECURSEn(ret = ConditionalExpression());
  } else {
    RECURSEn(ret = ConditionalExpression());
  }
  return ret;
}

// 6.8.7 UnaryExpression
AsmType* AsmJsParser::UnaryExpression() {
  AsmType* ret;
  if (Check('-')) {
    uint64_t uvalue;
    if (CheckForUnsigned(&uvalue)) {
      // TODO(bradnelson): was supposed to be 0x7fffffff, check errata.
      if (uvalue <= 0x80000000) {
        current_function_builder_->EmitI32Const(-static_cast<int32_t>(uvalue));
      } else {
        FAILn("Integer numeric literal out of range.");
      }
      ret = AsmType::Signed();
    } else {
      RECURSEn(ret = UnaryExpression());
      if (ret->IsA(AsmType::Int())) {
        TemporaryVariableScope tmp(this);
        current_function_builder_->EmitSetLocal(tmp.get());
        current_function_builder_->EmitI32Const(0);
        current_function_builder_->EmitGetLocal(tmp.get());
        current_function_builder_->Emit(kExprI32Sub);
        ret = AsmType::Intish();
      } else if (ret->IsA(AsmType::DoubleQ())) {
        current_function_builder_->Emit(kExprF64Neg);
        ret = AsmType::Double();
      } else if (ret->IsA(AsmType::FloatQ())) {
        current_function_builder_->Emit(kExprF32Neg);
        ret = AsmType::Floatish();
      } else {
        FAILn("expected int/double?/float?");
      }
    }
  } else if (Peek('+')) {
    call_coercion_ = AsmType::Double();
    call_coercion_position_ = scanner_.Position();
    scanner_.Next();  // Done late for correct position.
    RECURSEn(ret = UnaryExpression());
    // TODO(bradnelson): Generalize.
    if (ret->IsA(AsmType::Signed())) {
      current_function_builder_->Emit(kExprF64SConvertI32);
      ret = AsmType::Double();
    } else if (ret->IsA(AsmType::Unsigned())) {
      current_function_builder_->Emit(kExprF64UConvertI32);
      ret = AsmType::Double();
    } else if (ret->IsA(AsmType::DoubleQ())) {
      ret = AsmType::Double();
    } else if (ret->IsA(AsmType::FloatQ())) {
      current_function_builder_->Emit(kExprF64ConvertF32);
      ret = AsmType::Double();
    } else {
      FAILn("expected signed/unsigned/double?/float?");
    }
  } else if (Check('!')) {
    RECURSEn(ret = UnaryExpression());
    if (!ret->IsA(AsmType::Int())) {
      FAILn("expected int");
    }
    current_function_builder_->Emit(kExprI32Eqz);
  } else if (Check('~')) {
    if (Check('~')) {
      RECURSEn(ret = UnaryExpression());
      if (ret->IsA(AsmType::Double())) {
        current_function_builder_->Emit(kExprI32AsmjsSConvertF64);
      } else if (ret->IsA(AsmType::FloatQ())) {
        current_function_builder_->Emit(kExprI32AsmjsSConvertF32);
      } else {
        FAILn("expected double or float?");
      }
      ret = AsmType::Signed();
    } else {
      RECURSEn(ret = UnaryExpression());
      if (!ret->IsA(AsmType::Intish())) {
        FAILn("operator ~ expects intish");
      }
      current_function_builder_->EmitI32Const(0xffffffff);
      current_function_builder_->Emit(kExprI32Xor);
      ret = AsmType::Signed();
    }
  } else {
    RECURSEn(ret = CallExpression());
  }
  return ret;
}

// 6.8.8 MultaplicativeExpression
AsmType* AsmJsParser::MultiplicativeExpression() {
  uint64_t uvalue;
  if (CheckForUnsignedBelow(0x100000, &uvalue)) {
    if (Check('*')) {
      AsmType* a;
      RECURSEn(a = UnaryExpression());
      if (!a->IsA(AsmType::Int())) {
        FAILn("Expected int");
      }
      current_function_builder_->EmitI32Const(static_cast<int32_t>(uvalue));
      current_function_builder_->Emit(kExprI32Mul);
      return AsmType::Intish();
    }
    scanner_.Rewind();
  } else if (Check('-')) {
    if (CheckForUnsignedBelow(0x100000, &uvalue)) {
      current_function_builder_->EmitI32Const(-static_cast<int32_t>(uvalue));
      if (Check('*')) {
        AsmType* a;
        RECURSEn(a = UnaryExpression());
        if (!a->IsA(AsmType::Int())) {
          FAILn("Expected int");
        }
        current_function_builder_->Emit(kExprI32Mul);
        return AsmType::Intish();
      }
      return AsmType::Signed();
    }
    scanner_.Rewind();
  }
  AsmType* a;
  RECURSEn(a = UnaryExpression());
  for (;;) {
    if (Check('*')) {
      uint64_t uvalue;
      if (Check('-')) {
        if (CheckForUnsigned(&uvalue)) {
          if (uvalue >= 0x100000) {
            FAILn("Constant multiple out of range");
          }
          if (!a->IsA(AsmType::Int())) {
            FAILn("Integer multiply of expects int");
          }
          current_function_builder_->EmitI32Const(static_cast<int32_t>(uvalue));
          current_function_builder_->Emit(kExprI32Mul);
          return AsmType::Intish();
        }
        scanner_.Rewind();
      } else if (CheckForUnsigned(&uvalue)) {
        if (uvalue >= 0x100000) {
          FAILn("Constant multiple out of range");
        }
        if (!a->IsA(AsmType::Int())) {
          FAILn("Integer multiply of expects int");
        }
        current_function_builder_->EmitI32Const(static_cast<int32_t>(uvalue));
        current_function_builder_->Emit(kExprI32Mul);
        return AsmType::Intish();
      }
      AsmType* b;
      RECURSEn(b = UnaryExpression());
      if (a->IsA(AsmType::DoubleQ()) && b->IsA(AsmType::DoubleQ())) {
        current_function_builder_->Emit(kExprF64Mul);
        a = AsmType::Double();
      } else if (a->IsA(AsmType::FloatQ()) && b->IsA(AsmType::FloatQ())) {
        current_function_builder_->Emit(kExprF32Mul);
        a = AsmType::Floatish();
      } else {
        FAILn("expected doubles or floats");
      }
    } else if (Check('/')) {
      AsmType* b;
      RECURSEn(b = MultiplicativeExpression());
      if (a->IsA(AsmType::DoubleQ()) && b->IsA(AsmType::DoubleQ())) {
        current_function_builder_->Emit(kExprF64Div);
        a = AsmType::Double();
      } else if (a->IsA(AsmType::FloatQ()) && b->IsA(AsmType::FloatQ())) {
        current_function_builder_->Emit(kExprF32Div);
        a = AsmType::Floatish();
      } else if (a->IsA(AsmType::Signed()) && b->IsA(AsmType::Signed())) {
        current_function_builder_->Emit(kExprI32AsmjsDivS);
        a = AsmType::Intish();
      } else if (a->IsA(AsmType::Unsigned()) && b->IsA(AsmType::Unsigned())) {
        current_function_builder_->Emit(kExprI32AsmjsDivU);
        a = AsmType::Intish();
      } else {
        FAILn("expected doubles or floats");
      }
    } else if (Check('%')) {
      AsmType* b;
      RECURSEn(b = MultiplicativeExpression());
      if (a->IsA(AsmType::DoubleQ()) && b->IsA(AsmType::DoubleQ())) {
        current_function_builder_->Emit(kExprF64Mod);
        a = AsmType::Double();
      } else if (a->IsA(AsmType::Signed()) && b->IsA(AsmType::Signed())) {
        current_function_builder_->Emit(kExprI32AsmjsRemS);
        a = AsmType::Intish();
      } else if (a->IsA(AsmType::Unsigned()) && b->IsA(AsmType::Unsigned())) {
        current_function_builder_->Emit(kExprI32AsmjsRemU);
        a = AsmType::Intish();
      } else {
        FAILn("expected doubles or floats");
      }
    } else {
      break;
    }
  }
  return a;
}

// 6.8.9 AdditiveExpression
AsmType* AsmJsParser::AdditiveExpression() {
  AsmType* a;
  RECURSEn(a = MultiplicativeExpression());
  int n = 0;
  for (;;) {
    if (Check('+')) {
      AsmType* b;
      RECURSEn(b = MultiplicativeExpression());
      if (a->IsA(AsmType::Double()) && b->IsA(AsmType::Double())) {
        current_function_builder_->Emit(kExprF64Add);
        a = AsmType::Double();
      } else if (a->IsA(AsmType::FloatQ()) && b->IsA(AsmType::FloatQ())) {
        current_function_builder_->Emit(kExprF32Add);
        a = AsmType::Floatish();
      } else if (a->IsA(AsmType::Int()) && b->IsA(AsmType::Int())) {
        current_function_builder_->Emit(kExprI32Add);
        a = AsmType::Intish();
        n = 2;
      } else if (a->IsA(AsmType::Intish()) && b->IsA(AsmType::Intish())) {
        // TODO(bradnelson): b should really only be Int.
        // specialize intish to capture count.
        ++n;
        if (n > (1 << 20)) {
          FAILn("more than 2^20 additive values");
        }
        current_function_builder_->Emit(kExprI32Add);
      } else {
        FAILn("illegal types for +");
      }
    } else if (Check('-')) {
      AsmType* b;
      RECURSEn(b = MultiplicativeExpression());
      if (a->IsA(AsmType::Double()) && b->IsA(AsmType::Double())) {
        current_function_builder_->Emit(kExprF64Sub);
        a = AsmType::Double();
      } else if (a->IsA(AsmType::FloatQ()) && b->IsA(AsmType::FloatQ())) {
        current_function_builder_->Emit(kExprF32Sub);
        a = AsmType::Floatish();
      } else if (a->IsA(AsmType::Int()) && b->IsA(AsmType::Int())) {
        current_function_builder_->Emit(kExprI32Sub);
        a = AsmType::Intish();
        n = 2;
      } else if (a->IsA(AsmType::Intish()) && b->IsA(AsmType::Intish())) {
        // TODO(bradnelson): b should really only be Int.
        // specialize intish to capture count.
        ++n;
        if (n > (1 << 20)) {
          FAILn("more than 2^20 additive values");
        }
        current_function_builder_->Emit(kExprI32Sub);
      } else {
        FAILn("illegal types for +");
      }
    } else {
      break;
    }
  }
  return a;
}

// 6.8.10 ShiftExpression
AsmType* AsmJsParser::ShiftExpression() {
  AsmType* a = nullptr;
  RECURSEn(a = AdditiveExpression());
  for (;;) {
    switch (scanner_.Token()) {
// TODO(bradnelson): Implement backtracking to avoid emitting code
// for the x >>> 0 case (similar to what's there for |0).
#define HANDLE_CASE(op, opcode, name, result)                        \
  case TOK(op): {                                                    \
    EXPECT_TOKENn(TOK(op));                                          \
    AsmType* b = nullptr;                                            \
    RECURSEn(b = AdditiveExpression());                              \
    if (!(a->IsA(AsmType::Intish()) && b->IsA(AsmType::Intish()))) { \
      FAILn("Expected intish for operator " #name ".");              \
    }                                                                \
    current_function_builder_->Emit(kExpr##opcode);                  \
    a = AsmType::result();                                           \
    continue;                                                        \
  }
      HANDLE_CASE(SHL, I32Shl, "<<", Signed);
      HANDLE_CASE(SAR, I32ShrS, ">>", Signed);
      HANDLE_CASE(SHR, I32ShrU, ">>>", Unsigned);
#undef HANDLE_CASE
      default:
        return a;
    }
  }
}

// 6.8.11 RelationalExpression
AsmType* AsmJsParser::RelationalExpression() {
  AsmType* a = nullptr;
  RECURSEn(a = ShiftExpression());
  for (;;) {
    switch (scanner_.Token()) {
#define HANDLE_CASE(op, sop, uop, dop, fop, name)                             \
  case op: {                                                                  \
    EXPECT_TOKENn(op);                                                        \
    AsmType* b = nullptr;                                                     \
    RECURSEn(b = ShiftExpression());                                          \
    if (a->IsA(AsmType::Signed()) && b->IsA(AsmType::Signed())) {             \
      current_function_builder_->Emit(kExpr##sop);                            \
    } else if (a->IsA(AsmType::Unsigned()) && b->IsA(AsmType::Unsigned())) {  \
      current_function_builder_->Emit(kExpr##uop);                            \
    } else if (a->IsA(AsmType::Double()) && b->IsA(AsmType::Double())) {      \
      current_function_builder_->Emit(kExpr##dop);                            \
    } else if (a->IsA(AsmType::Float()) && b->IsA(AsmType::Float())) {        \
      current_function_builder_->Emit(kExpr##fop);                            \
    } else {                                                                  \
      FAILn("Expected signed, unsigned, double, or float for operator " #name \
            ".");                                                             \
    }                                                                         \
    a = AsmType::Int();                                                       \
    continue;                                                                 \
  }
      HANDLE_CASE('<', I32LtS, I32LtU, F64Lt, F32Lt, "<");
      HANDLE_CASE(TOK(LE), I32LeS, I32LeU, F64Le, F32Le, "<=");
      HANDLE_CASE('>', I32GtS, I32GtU, F64Gt, F32Gt, ">");
      HANDLE_CASE(TOK(GE), I32GeS, I32GeU, F64Ge, F32Ge, ">=");
#undef HANDLE_CASE
      default:
        return a;
    }
  }
}

// 6.8.12 EqualityExpression
AsmType* AsmJsParser::EqualityExpression() {
  AsmType* a = nullptr;
  RECURSEn(a = RelationalExpression());
  for (;;) {
    switch (scanner_.Token()) {
#define HANDLE_CASE(op, sop, uop, dop, fop, name)                             \
  case op: {                                                                  \
    EXPECT_TOKENn(op);                                                        \
    AsmType* b = nullptr;                                                     \
    RECURSEn(b = RelationalExpression());                                     \
    if (a->IsA(AsmType::Signed()) && b->IsA(AsmType::Signed())) {             \
      current_function_builder_->Emit(kExpr##sop);                            \
    } else if (a->IsA(AsmType::Unsigned()) && b->IsA(AsmType::Unsigned())) {  \
      current_function_builder_->Emit(kExpr##uop);                            \
    } else if (a->IsA(AsmType::Double()) && b->IsA(AsmType::Double())) {      \
      current_function_builder_->Emit(kExpr##dop);                            \
    } else if (a->IsA(AsmType::Float()) && b->IsA(AsmType::Float())) {        \
      current_function_builder_->Emit(kExpr##fop);                            \
    } else {                                                                  \
      FAILn("Expected signed, unsigned, double, or float for operator " #name \
            ".");                                                             \
    }                                                                         \
    a = AsmType::Int();                                                       \
    continue;                                                                 \
  }
      HANDLE_CASE(TOK(EQ), I32Eq, I32Eq, F64Eq, F32Eq, "==");
      HANDLE_CASE(TOK(NE), I32Ne, I32Ne, F64Ne, F32Ne, "!=");
#undef HANDLE_CASE
      default:
        return a;
    }
  }
}

// 6.8.13 BitwiseANDExpression
AsmType* AsmJsParser::BitwiseANDExpression() {
  AsmType* a = nullptr;
  RECURSEn(a = EqualityExpression());
  while (Check('&')) {
    AsmType* b = nullptr;
    RECURSEn(b = EqualityExpression());
    if (a->IsA(AsmType::Intish()) && b->IsA(AsmType::Intish())) {
      current_function_builder_->Emit(kExprI32And);
      a = AsmType::Signed();
    } else {
      FAILn("Expected intish for operator &.");
    }
  }
  return a;
}

// 6.8.14 BitwiseXORExpression
AsmType* AsmJsParser::BitwiseXORExpression() {
  AsmType* a = nullptr;
  RECURSEn(a = BitwiseANDExpression());
  while (Check('^')) {
    AsmType* b = nullptr;
    RECURSEn(b = BitwiseANDExpression());
    if (a->IsA(AsmType::Intish()) && b->IsA(AsmType::Intish())) {
      current_function_builder_->Emit(kExprI32Xor);
      a = AsmType::Signed();
    } else {
      FAILn("Expected intish for operator &.");
    }
  }
  return a;
}

// 6.8.15 BitwiseORExpression
AsmType* AsmJsParser::BitwiseORExpression() {
  AsmType* a = nullptr;
  RECURSEn(a = BitwiseXORExpression());
  while (Check('|')) {
    // TODO(bradnelson): Make it prettier.
    AsmType* b = nullptr;
    bool zero = false;
    int old_pos;
    size_t old_code;
    if (CheckForZero()) {
      old_pos = scanner_.GetPosition();
      old_code = current_function_builder_->GetPosition();
      scanner_.Rewind();
      zero = true;
    }
    RECURSEn(b = BitwiseXORExpression());
    // Handle |0 specially.
    if (zero && old_pos == scanner_.GetPosition()) {
      current_function_builder_->StashCode(nullptr, old_code);
      a = AsmType::Signed();
      continue;
    }
    if (a->IsA(AsmType::Intish()) && b->IsA(AsmType::Intish())) {
      current_function_builder_->Emit(kExprI32Ior);
      a = AsmType::Signed();
    } else {
      FAILn("Expected intish for operator |.");
    }
  }
  return a;
}

// 6.8.16 ConditionalExpression
AsmType* AsmJsParser::ConditionalExpression() {
  AsmType* test = nullptr;
  RECURSEn(test = BitwiseORExpression());
  if (Check('?')) {
    if (!test->IsA(AsmType::Int())) {
      FAILn("Expected int in condition of ternary operator.");
    }
    current_function_builder_->EmitWithU8(kExprIf, kLocalI32);
    size_t fixup = current_function_builder_->GetPosition() -
                   1;  // Assumes encoding knowledge.
    AsmType* cons = nullptr;
    RECURSEn(cons = AssignmentExpression());
    current_function_builder_->Emit(kExprElse);
    EXPECT_TOKENn(':');
    AsmType* alt = nullptr;
    RECURSEn(alt = AssignmentExpression());
    current_function_builder_->Emit(kExprEnd);
    if (cons->IsA(AsmType::Int()) && alt->IsA(AsmType::Int())) {
      current_function_builder_->FixupByte(fixup, kLocalI32);
      return AsmType::Int();
    } else if (cons->IsA(AsmType::Double()) && alt->IsA(AsmType::Double())) {
      current_function_builder_->FixupByte(fixup, kLocalF64);
      return AsmType::Double();
    } else if (cons->IsA(AsmType::Float()) && alt->IsA(AsmType::Float())) {
      current_function_builder_->FixupByte(fixup, kLocalF32);
      return AsmType::Float();
    } else {
      FAILn("Type mismatch in ternary operator.");
    }
  } else {
    return test;
  }
}

// 6.8.17 ParenthesiedExpression
AsmType* AsmJsParser::ParenthesizedExpression() {
  call_coercion_ = nullptr;
  AsmType* ret;
  EXPECT_TOKENn('(');
  RECURSEn(ret = Expression(nullptr));
  EXPECT_TOKENn(')');
  return ret;
}

// 6.9 ValidateCall
AsmType* AsmJsParser::ValidateCall() {
  AsmType* return_type = call_coercion_;
  call_coercion_ = nullptr;
  int call_pos = static_cast<int>(scanner_.Position());
  int to_number_pos = static_cast<int>(call_coercion_position_);
  AsmJsScanner::token_t function_name = Consume();

  // Distinguish between ordinary function calls and function table calls. In
  // both cases we might be seeing the {function_name} for the first time and
  // hence allocate a {VarInfo} here, all subsequent uses of the same name then
  // need to match the information stored at this point.
  // TODO(mstarzinger): Consider using Chromiums base::Optional instead.
  std::unique_ptr<TemporaryVariableScope> tmp;
  if (Check('[')) {
    RECURSEn(EqualityExpression());
    EXPECT_TOKENn('&');
    uint64_t mask = 0;
    if (!CheckForUnsigned(&mask)) {
      FAILn("Expected mask literal");
    }
    if (mask > 0x7fffffff) {
      FAILn("Expected power of 2 mask");
    }
    if (!base::bits::IsPowerOfTwo32(static_cast<uint32_t>(1 + mask))) {
      FAILn("Expected power of 2 mask");
    }
    current_function_builder_->EmitI32Const(static_cast<uint32_t>(mask));
    current_function_builder_->Emit(kExprI32And);
    EXPECT_TOKENn(']');
    VarInfo* function_info = GetVarInfo(function_name);
    if (function_info->kind == VarKind::kUnused) {
      function_info->kind = VarKind::kTable;
      function_info->mask = static_cast<int32_t>(mask);
      function_info->index = module_builder_->AllocateIndirectFunctions(
          static_cast<uint32_t>(mask + 1));
    } else {
      if (function_info->kind != VarKind::kTable) {
        FAILn("Expected call table");
      }
      if (function_info->mask != static_cast<int32_t>(mask)) {
        FAILn("Mask size mismatch");
      }
    }
    current_function_builder_->EmitI32Const(function_info->index);
    current_function_builder_->Emit(kExprI32Add);
    // We have to use a temporary for the correct order of evaluation.
    tmp.reset(new TemporaryVariableScope(this));
    current_function_builder_->EmitSetLocal(tmp.get()->get());
    // The position of function table calls is after the table lookup.
    call_pos = static_cast<int>(scanner_.Position());
  } else {
    VarInfo* function_info = GetVarInfo(function_name);
    if (function_info->kind == VarKind::kUnused) {
      function_info->kind = VarKind::kFunction;
      function_info->function_builder = module_builder_->AddFunction();
      function_info->index = function_info->function_builder->func_index();
    } else {
      if (function_info->kind != VarKind::kFunction &&
          function_info->kind < VarKind::kImportedFunction) {
        FAILn("Expected function as call target");
      }
    }
  }

  // Parse argument list and gather types.
  std::vector<AsmType*> param_types;
  ZoneVector<AsmType*> param_specific_types(zone());
  EXPECT_TOKENn('(');
  while (!failed_ && !Peek(')')) {
    AsmType* t;
    RECURSEn(t = AssignmentExpression());
    param_specific_types.push_back(t);
    if (t->IsA(AsmType::Int())) {
      param_types.push_back(AsmType::Int());
    } else if (t->IsA(AsmType::Float())) {
      param_types.push_back(AsmType::Float());
    } else if (t->IsA(AsmType::Double())) {
      param_types.push_back(AsmType::Double());
    } else {
      std::string a = t->Name();
      FAILn("Bad function argument type");
    }
    if (!Peek(')')) {
      EXPECT_TOKENn(',');
    }
  }
  EXPECT_TOKENn(')');

  // We potentially use lookahead in order to determine the return type in case
  // it is not yet clear from the call context.
  // TODO(mstarzinger,6183): Several issues with look-ahead are known. Fix!
  // TODO(bradnelson): clarify how this binds, and why only float?
  if (Peek('|') &&
      (return_type == nullptr || return_type->IsA(AsmType::Float()))) {
    to_number_pos = static_cast<int>(scanner_.Position());
    return_type = AsmType::Signed();
  } else if (return_type == nullptr) {
    to_number_pos = call_pos;  // No conversion.
    return_type = AsmType::Void();
  }

  // Compute function type and signature based on gathered types.
  AsmType* function_type = AsmType::Function(zone(), return_type);
  for (auto t : param_types) {
    function_type->AsFunctionType()->AddArgument(t);
  }
  FunctionSig* sig = ConvertSignature(return_type, param_types);
  if (sig == nullptr) {
    FAILn("Invalid function signature");
  }
  uint32_t signature_index = module_builder_->AddSignature(sig);

  // Emit actual function invocation depending on the kind. At this point we
  // also determined the complete function type and can perform checking against
  // the expected type or update the expected type in case of first occurrence.
  // Reload {VarInfo} as table might have grown.
  VarInfo* function_info = GetVarInfo(function_name);
  if (function_info->kind == VarKind::kImportedFunction) {
    for (auto t : param_specific_types) {
      if (!t->IsA(AsmType::Extern())) {
        FAILn("Imported function args must be type extern");
      }
    }
    if (return_type->IsA(AsmType::Float())) {
      FAILn("Imported function can't be called as float");
    }
    DCHECK(function_info->import != nullptr);
    // TODO(bradnelson): Factor out.
    uint32_t cache_index = function_info->import->cache.FindOrInsert(sig);
    uint32_t index;
    if (cache_index >= function_info->import->cache_index.size()) {
      index = module_builder_->AddImport(
          function_info->import->function_name,
          static_cast<uint32_t>(function_info->import->function_name_size),
          sig);
      function_info->import->cache_index.push_back(index);
    } else {
      index = function_info->import->cache_index[cache_index];
    }
    current_function_builder_->AddAsmWasmOffset(call_pos, to_number_pos);
    current_function_builder_->Emit(kExprCallFunction);
    current_function_builder_->EmitVarUint(index);
  } else if (function_info->kind > VarKind::kImportedFunction) {
    AsmCallableType* callable = function_info->type->AsCallableType();
    if (!callable) {
      FAILn("Expected callable function");
    }
    // TODO(bradnelson): Refactor AsmType to not need this.
    if (callable->CanBeInvokedWith(return_type, param_specific_types)) {
      // Return type ok.
    } else if (return_type->IsA(AsmType::Void()) &&
               callable->CanBeInvokedWith(AsmType::Float(),
                                          param_specific_types)) {
      return_type = AsmType::Float();
    } else if (return_type->IsA(AsmType::Void()) &&
               callable->CanBeInvokedWith(AsmType::Double(),
                                          param_specific_types)) {
      return_type = AsmType::Double();
    } else if (return_type->IsA(AsmType::Void()) &&
               callable->CanBeInvokedWith(AsmType::Signed(),
                                          param_specific_types)) {
      return_type = AsmType::Signed();
    } else {
      FAILn("Function use doesn't match definition");
    }
    switch (function_info->kind) {
#define V(name, Name, op, sig)           \
  case VarKind::kMath##Name:             \
    current_function_builder_->Emit(op); \
    break;
      STDLIB_MATH_FUNCTION_MONOMORPHIC_LIST(V)
#undef V
#define V(name, Name, op, sig)                                    \
  case VarKind::kMath##Name:                                      \
    if (param_specific_types[0]->IsA(AsmType::DoubleQ())) {       \
      current_function_builder_->Emit(kExprF64##Name);            \
    } else if (param_specific_types[0]->IsA(AsmType::FloatQ())) { \
      current_function_builder_->Emit(kExprF32##Name);            \
    } else {                                                      \
      UNREACHABLE();                                              \
    }                                                             \
    break;
      STDLIB_MATH_FUNCTION_CEIL_LIKE_LIST(V)
#undef V
      case VarKind::kMathMin:
      case VarKind::kMathMax:
        if (param_specific_types[0]->IsA(AsmType::Double())) {
          for (size_t i = 1; i < param_specific_types.size(); ++i) {
            if (function_info->kind == VarKind::kMathMin) {
              current_function_builder_->Emit(kExprF64Min);
            } else {
              current_function_builder_->Emit(kExprF64Max);
            }
          }
        } else if (param_specific_types[0]->IsA(AsmType::Float())) {
          // NOTE: Not technically part of the asm.js spec, but Firefox
          // accepts it.
          for (size_t i = 1; i < param_specific_types.size(); ++i) {
            if (function_info->kind == VarKind::kMathMin) {
              current_function_builder_->Emit(kExprF32Min);
            } else {
              current_function_builder_->Emit(kExprF32Max);
            }
          }
        } else if (param_specific_types[0]->IsA(AsmType::Int())) {
          TemporaryVariableScope tmp_x(this);
          TemporaryVariableScope tmp_y(this);
          for (size_t i = 1; i < param_specific_types.size(); ++i) {
            current_function_builder_->EmitSetLocal(tmp_x.get());
            current_function_builder_->EmitTeeLocal(tmp_y.get());
            current_function_builder_->EmitGetLocal(tmp_x.get());
            if (function_info->kind == VarKind::kMathMin) {
              current_function_builder_->Emit(kExprI32GeS);
            } else {
              current_function_builder_->Emit(kExprI32LeS);
            }
            current_function_builder_->EmitWithU8(kExprIf, kLocalI32);
            current_function_builder_->EmitGetLocal(tmp_x.get());
            current_function_builder_->Emit(kExprElse);
            current_function_builder_->EmitGetLocal(tmp_y.get());
            current_function_builder_->Emit(kExprEnd);
          }
        } else {
          UNREACHABLE();
        }
        break;

      case VarKind::kMathAbs:
        if (param_specific_types[0]->IsA(AsmType::Signed())) {
          TemporaryVariableScope tmp(this);
          current_function_builder_->EmitTeeLocal(tmp.get());
          current_function_builder_->Emit(kExprI32Clz);
          current_function_builder_->EmitWithU8(kExprIf, kLocalI32);
          current_function_builder_->EmitGetLocal(tmp.get());
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitI32Const(0);
          current_function_builder_->EmitGetLocal(tmp.get());
          current_function_builder_->Emit(kExprI32Sub);
          current_function_builder_->Emit(kExprEnd);
        } else if (param_specific_types[0]->IsA(AsmType::DoubleQ())) {
          current_function_builder_->Emit(kExprF64Abs);
        } else if (param_specific_types[0]->IsA(AsmType::FloatQ())) {
          current_function_builder_->Emit(kExprF32Abs);
        } else {
          UNREACHABLE();
        }
        break;

      case VarKind::kMathFround:
        if (param_specific_types[0]->IsA(AsmType::DoubleQ())) {
          current_function_builder_->Emit(kExprF32ConvertF64);
        } else {
          DCHECK(param_specific_types[0]->IsA(AsmType::FloatQ()));
        }
        break;

      default:
        UNREACHABLE();
    }
  } else {
    DCHECK(function_info->kind == VarKind::kFunction ||
           function_info->kind == VarKind::kTable);
    if (function_info->type->IsA(AsmType::None())) {
      function_info->type = function_type;
    } else {
      AsmCallableType* callable = function_info->type->AsCallableType();
      if (!callable ||
          !callable->CanBeInvokedWith(return_type, param_specific_types)) {
        FAILn("Function use doesn't match definition");
      }
    }
    if (function_info->kind == VarKind::kTable) {
      current_function_builder_->EmitGetLocal(tmp.get()->get());
      current_function_builder_->AddAsmWasmOffset(call_pos, to_number_pos);
      current_function_builder_->Emit(kExprCallIndirect);
      current_function_builder_->EmitVarUint(signature_index);
      current_function_builder_->EmitVarUint(0);  // table index
    } else {
      current_function_builder_->AddAsmWasmOffset(call_pos, to_number_pos);
      current_function_builder_->Emit(kExprCallFunction);
      current_function_builder_->EmitDirectCallIndex(function_info->index);
    }
  }

  return return_type;
}

// 6.9 ValidateCall - helper
bool AsmJsParser::PeekCall() {
  if (!scanner_.IsGlobal()) {
    return false;
  }
  if (GetVarInfo(scanner_.Token())->kind == VarKind::kFunction) {
    return true;
  }
  if (GetVarInfo(scanner_.Token())->kind >= VarKind::kImportedFunction) {
    return true;
  }
  if (GetVarInfo(scanner_.Token())->kind == VarKind::kUnused ||
      GetVarInfo(scanner_.Token())->kind == VarKind::kTable) {
    scanner_.Next();
    if (Peek('(') || Peek('[')) {
      scanner_.Rewind();
      return true;
    }
    scanner_.Rewind();
  }
  return false;
}

// 6.10 ValidateHeapAccess
void AsmJsParser::ValidateHeapAccess() {
  VarInfo* info = GetVarInfo(Consume());
  int32_t size = info->type->ElementSizeInBytes();
  EXPECT_TOKEN('[');
  uint64_t offset;
  if (CheckForUnsigned(&offset)) {
    // TODO(bradnelson): Check more things.
    if (offset > 0x7fffffff || offset * size > 0x7fffffff) {
      FAIL("Heap access out of range");
    }
    if (Check(']')) {
      current_function_builder_->EmitI32Const(
          static_cast<uint32_t>(offset * size));
      // NOTE: This has to happen here to work recursively.
      heap_access_type_ = info->type;
      return;
    } else {
      scanner_.Rewind();
    }
  }
  AsmType* index_type;
  if (info->type->IsA(AsmType::Int8Array()) ||
      info->type->IsA(AsmType::Uint8Array())) {
    RECURSE(index_type = Expression(nullptr));
  } else {
    RECURSE(index_type = AdditiveExpression());
    EXPECT_TOKEN(TOK(SAR));
    uint64_t shift;
    if (!CheckForUnsigned(&shift)) {
      FAIL("Expected shift of word size");
    }
    if (shift > 3) {
      FAIL("Expected valid heap access shift");
    }
    if ((1 << shift) != size) {
      FAIL("Expected heap access shift to match heap view");
    }
    // Mask bottom bits to match asm.js behavior.
    current_function_builder_->EmitI32Const(~(size - 1));
    current_function_builder_->Emit(kExprI32And);
  }
  if (!index_type->IsA(AsmType::Intish())) {
    FAIL("Expected intish index");
  }
  EXPECT_TOKEN(']');
  // NOTE: This has to happen here to work recursively.
  heap_access_type_ = info->type;
}

// 6.11 ValidateFloatCoercion
void AsmJsParser::ValidateFloatCoercion() {
  if (!scanner_.IsGlobal() ||
      !GetVarInfo(scanner_.Token())->type->IsA(stdlib_fround_)) {
    FAIL("Expected fround");
  }
  scanner_.Next();
  EXPECT_TOKEN('(');
  call_coercion_ = AsmType::Float();
  // NOTE: The coercion position to float is not observable from JavaScript,
  // because imported functions are not allowed to have float return type.
  call_coercion_position_ = scanner_.Position();
  AsmType* ret;
  RECURSE(ret = ValidateExpression());
  if (ret->IsA(AsmType::Floatish())) {
    // Do nothing, as already a float.
  } else if (ret->IsA(AsmType::DoubleQ())) {
    current_function_builder_->Emit(kExprF32ConvertF64);
  } else if (ret->IsA(AsmType::Signed())) {
    current_function_builder_->Emit(kExprF32SConvertI32);
  } else if (ret->IsA(AsmType::Unsigned())) {
    current_function_builder_->Emit(kExprF32UConvertI32);
  } else {
    FAIL("Illegal conversion to float");
  }
  EXPECT_TOKEN(')');
}

void AsmJsParser::GatherCases(std::vector<int32_t>* cases) {
  int start = scanner_.GetPosition();
  int depth = 0;
  for (;;) {
    if (Peek('{')) {
      ++depth;
    } else if (Peek('}')) {
      --depth;
      if (depth <= 0) {
        break;
      }
    } else if (depth == 1 && Peek(TOK(case))) {
      scanner_.Next();
      int32_t value;
      uint64_t uvalue;
      if (Check('-')) {
        if (!CheckForUnsigned(&uvalue)) {
          break;
        }
        value = -static_cast<int32_t>(uvalue);
      } else {
        if (!CheckForUnsigned(&uvalue)) {
          break;
        }
        value = static_cast<int32_t>(uvalue);
      }
      cases->push_back(value);
    } else if (Peek(AsmJsScanner::kEndOfInput)) {
      break;
    }
    scanner_.Next();
  }
  scanner_.Seek(start);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
