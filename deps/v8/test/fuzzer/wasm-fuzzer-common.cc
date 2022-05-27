// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm-fuzzer-common.h"

#include <ctime>

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-metrics.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-feature-flags.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

// Compile a baseline module. We pass a pointer to a max step counter and a
// nondeterminsm flag that are updated during execution by Liftoff.
Handle<WasmModuleObject> CompileReferenceModule(Zone* zone, Isolate* isolate,
                                                ModuleWireBytes wire_bytes,
                                                ErrorThrower* thrower,
                                                int32_t* max_steps,
                                                int32_t* nondeterminism) {
  // Create the native module.
  std::shared_ptr<NativeModule> native_module;
  constexpr bool kNoVerifyFunctions = false;
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(isolate);
  ModuleResult module_res = DecodeWasmModule(
      enabled_features, wire_bytes.start(), wire_bytes.end(),
      kNoVerifyFunctions, ModuleOrigin::kWasmOrigin, isolate->counters(),
      isolate->metrics_recorder(), v8::metrics::Recorder::ContextId::Empty(),
      DecodingMethod::kSync, GetWasmEngine()->allocator());
  CHECK(module_res.ok());
  std::shared_ptr<WasmModule> module = module_res.value();
  CHECK_NOT_NULL(module);
  native_module =
      GetWasmEngine()->NewNativeModule(isolate, enabled_features, module, 0);
  native_module->SetWireBytes(
      base::OwnedVector<uint8_t>::Of(wire_bytes.module_bytes()));

  // Compile all functions with Liftoff.
  WasmCodeRefScope code_ref_scope;
  auto env = native_module->CreateCompilationEnv();
  for (size_t i = module->num_imported_functions; i < module->functions.size();
       ++i) {
    auto& func = module->functions[i];
    base::Vector<const uint8_t> func_code = wire_bytes.GetFunctionBytes(&func);
    FunctionBody func_body(func.sig, func.code.offset(), func_code.begin(),
                           func_code.end());
    auto result =
        ExecuteLiftoffCompilation(&env, func_body,
                                  LiftoffOptions{}
                                      .set_func_index(func.func_index)
                                      .set_for_debugging(kForDebugging)
                                      .set_max_steps(max_steps)
                                      .set_nondeterminism(nondeterminism));
    native_module->PublishCode(
        native_module->AddCompiledCode(std::move(result)));
  }

  // Create the module object.
  constexpr base::Vector<const char> kNoSourceUrl;
  Handle<Script> script =
      GetWasmEngine()->GetOrCreateScript(isolate, native_module, kNoSourceUrl);
  Handle<FixedArray> export_wrappers = isolate->factory()->NewFixedArray(
      static_cast<int>(module->functions.size()));
  return WasmModuleObject::New(isolate, std::move(native_module), script,
                               export_wrappers);
}

void InterpretAndExecuteModule(i::Isolate* isolate,
                               Handle<WasmModuleObject> module_object,
                               Handle<WasmModuleObject> module_ref,
                               int32_t* max_steps, int32_t* nondeterminism) {
  // We do not instantiate the module if there is a start function, because a
  // start function can contain an infinite loop which we cannot handle.
  if (module_object->module()->start_function_index >= 0) return;

  HandleScope handle_scope(isolate);  // Avoid leaking handles.
  Handle<WasmInstanceObject> instance;

  // Try to instantiate, return if it fails.
  {
    ErrorThrower thrower(isolate, "WebAssembly Instantiation");
    if (!GetWasmEngine()
             ->SyncInstantiate(isolate, &thrower, module_object, {},
                               {})  // no imports & memory
             .ToHandle(&instance)) {
      isolate->clear_pending_exception();
      thrower.Reset();  // Ignore errors.
      return;
    }
  }

  // Get the "main" exported function. Do nothing if it does not exist.
  Handle<WasmExportedFunction> main_function;
  if (!testing::GetExportedFunction(isolate, instance, "main")
           .ToHandle(&main_function)) {
    return;
  }

  base::OwnedVector<Handle<Object>> compiled_args =
      testing::MakeDefaultArguments(isolate, main_function->sig());
  bool exception_ref = false;
  bool exception = false;
  int32_t result_ref = 0;
  int32_t result = 0;

  if (module_ref.is_null()) {
    base::OwnedVector<WasmValue> arguments =
        testing::MakeDefaultInterpreterArguments(isolate, main_function->sig());

    // Now interpret.
    testing::WasmInterpretationResult interpreter_result =
        testing::InterpretWasmModule(isolate, instance,
                                     main_function->function_index(),
                                     arguments.begin());
    if (interpreter_result.failed()) return;

    // The WebAssembly spec allows the sign bit of NaN to be non-deterministic.
    // This sign bit can make the difference between an infinite loop and
    // terminating code. With possible non-determinism we cannot guarantee that
    // the generated code will not go into an infinite loop and cause a timeout
    // in Clusterfuzz. Therefore we do not execute the generated code if the
    // result may be non-deterministic.
    if (interpreter_result.possible_nondeterminism()) return;
    if (interpreter_result.finished()) {
      result_ref = interpreter_result.result();
    } else {
      DCHECK(interpreter_result.trapped());
      exception_ref = true;
    }
    // Reset the instance before the test run.
    {
      ErrorThrower thrower(isolate, "Second Instantiation");
      // We instantiated before, so the second instantiation must also succeed:
      CHECK(GetWasmEngine()
                ->SyncInstantiate(isolate, &thrower, module_object, {},
                                  {})  // no imports & memory
                .ToHandle(&instance));
    }
  } else {
    Handle<WasmInstanceObject> instance_ref;
    {
      ErrorThrower thrower(isolate, "WebAssembly Instantiation");
      // We instantiated before, so the second instantiation must also succeed:
      CHECK(GetWasmEngine()
                ->SyncInstantiate(isolate, &thrower, module_ref, {},
                                  {})  // no imports & memory
                .ToHandle(&instance_ref));
    }
    result_ref = testing::CallWasmFunctionForTesting(
        isolate, instance_ref, "main", static_cast<int>(compiled_args.size()),
        compiled_args.begin(), &exception_ref);
    // Reached max steps, do not try to execute the test module as it might
    // never terminate.
    if (*max_steps == 0) return;
    // If there is nondeterminism, we cannot guarantee the behavior of the test
    // module, and in particular it may not terminate.
    if (*nondeterminism != 0) return;
  }

  result = testing::CallWasmFunctionForTesting(
      isolate, instance, "main", static_cast<int>(compiled_args.size()),
      compiled_args.begin(), &exception);

  if (exception_ref != exception) {
    const char* exception_text[] = {"no exception", "exception"};
    FATAL("expected: %s; got: %s", exception_text[exception_ref],
          exception_text[exception]);
  }

  if (!exception) {
    CHECK_EQ(result_ref, result);
  }
}

namespace {

struct PrintSig {
  const size_t num;
  const std::function<ValueType(size_t)> getter;
};

PrintSig PrintParameters(const FunctionSig* sig) {
  return {sig->parameter_count(), [=](size_t i) { return sig->GetParam(i); }};
}

PrintSig PrintReturns(const FunctionSig* sig) {
  return {sig->return_count(), [=](size_t i) { return sig->GetReturn(i); }};
}

std::string HeapTypeToConstantName(HeapType heap_type) {
  switch (heap_type.representation()) {
    case HeapType::kFunc:
      return "kWasmFuncRef";
    case HeapType::kEq:
      return "kWasmEqRef";
    case HeapType::kI31:
      return "kWasmI31Ref";
    case HeapType::kData:
      return "kWasmDataRef";
    case HeapType::kArray:
      return "kWasmArrayRef";
    case HeapType::kAny:
      return "kWasmAnyRef";
    case HeapType::kBottom:
      UNREACHABLE();
    default:
      return std::to_string(heap_type.ref_index());
  }
}

std::string ValueTypeToConstantName(ValueType type) {
  switch (type.kind()) {
    case kI8:
      return "kWasmI8";
    case kI16:
      return "kWasmI16";
    case kI32:
      return "kWasmI32";
    case kI64:
      return "kWasmI64";
    case kF32:
      return "kWasmF32";
    case kF64:
      return "kWasmF64";
    case kS128:
      return "kWasmS128";
    case kOptRef:
      switch (type.heap_representation()) {
        case HeapType::kFunc:
          return "kWasmFuncRef";
        case HeapType::kEq:
          return "kWasmEqRef";
        case HeapType::kAny:
          return "kWasmAnyRef";
        case HeapType::kBottom:
          UNREACHABLE();
        case HeapType::kData:
        case HeapType::kArray:
        case HeapType::kI31:
        default:
          return "wasmOptRefType(" + HeapTypeToConstantName(type.heap_type()) +
                 ")";
      }
    case kRef:
      return "wasmRefType(" + HeapTypeToConstantName(type.heap_type()) + ")";
    case kRtt:
    case kVoid:
    case kBottom:
      UNREACHABLE();
  }
}

std::ostream& operator<<(std::ostream& os, const PrintSig& print) {
  os << "[";
  for (size_t i = 0; i < print.num; ++i) {
    os << (i == 0 ? "" : ", ") << ValueTypeToConstantName(print.getter(i));
  }
  return os << "]";
}

struct PrintName {
  WasmName name;
  PrintName(ModuleWireBytes wire_bytes, WireBytesRef ref)
      : name(wire_bytes.GetNameOrNull(ref)) {}
};
std::ostream& operator<<(std::ostream& os, const PrintName& name) {
  return os.put('\'').write(name.name.begin(), name.name.size()).put('\'');
}

// An interface for WasmFullDecoder used to decode initializer expressions. As
// opposed to the one in src/wasm/, this emits {WasmInitExpr} as opposed to a
// {WasmValue}.
class InitExprInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kFullValidation;
  static constexpr DecodingMode decoding_mode = kInitExpression;

  struct Value : public ValueBase<validate> {
    WasmInitExpr init_expr;

    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };

  using Control = ControlBase<Value, validate>;
  using FullDecoder =
      WasmFullDecoder<validate, InitExprInterface, decoding_mode>;

  explicit InitExprInterface(Zone* zone) : zone_(zone) {}

#define EMPTY_INTERFACE_FUNCTION(name, ...) \
  V8_INLINE void name(FullDecoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_META_FUNCTIONS(EMPTY_INTERFACE_FUNCTION)
#undef EMPTY_INTERFACE_FUNCTION
#define UNREACHABLE_INTERFACE_FUNCTION(name, ...) \
  V8_INLINE void name(FullDecoder* decoder, ##__VA_ARGS__) { UNREACHABLE(); }
  INTERFACE_NON_CONSTANT_FUNCTIONS(UNREACHABLE_INTERFACE_FUNCTION)
#undef UNREACHABLE_INTERFACE_FUNCTION

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    result->init_expr = WasmInitExpr(value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    result->init_expr = WasmInitExpr(value);
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    result->init_expr = WasmInitExpr(value);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    result->init_expr = WasmInitExpr(value);
  }

  void S128Const(FullDecoder* decoder, Simd128Immediate<validate>& imm,
                 Value* result) {
    result->init_expr = WasmInitExpr(imm.value);
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
    // TODO(12089): Implement.
    UNIMPLEMENTED();
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    result->init_expr = WasmInitExpr::RefNullConst(type.heap_representation());
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    result->init_expr = WasmInitExpr::RefFuncConst(function_index);
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    result->init_expr = WasmInitExpr::GlobalGet(imm.index);
  }

  void StructNewWithRtt(FullDecoder* decoder,
                        const StructIndexImmediate<validate>& imm,
                        const Value& rtt, const Value args[], Value* result) {
    ZoneVector<WasmInitExpr>* elements =
        zone_->New<ZoneVector<WasmInitExpr>>(zone_);
    for (size_t i = 0; i < imm.struct_type->field_count(); i++) {
      elements->push_back(args[i].init_expr);
    }
    bool nominal = decoder->module_->has_supertype(imm.index);

    if (!nominal) elements->push_back(rtt.init_expr);

    result->init_expr =
        nominal ? WasmInitExpr::StructNew(imm.index, elements)
                : WasmInitExpr::StructNewWithRtt(imm.index, elements);
  }

  void StructNewDefault(FullDecoder* decoder,
                        const StructIndexImmediate<validate>& imm,
                        const Value& rtt, Value* result) {
    bool nominal = decoder->module_->has_supertype(imm.index);
    result->init_expr = nominal ? WasmInitExpr::StructNewDefault(imm.index)
                                : WasmInitExpr::StructNewDefaultWithRtt(
                                      zone_, imm.index, rtt.init_expr);
  }

  void ArrayInit(FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
                 const base::Vector<Value>& elements, const Value& rtt,
                 Value* result) {
    ZoneVector<WasmInitExpr>* args =
        zone_->New<ZoneVector<WasmInitExpr>>(zone_);
    for (Value expr : elements) args->push_back(expr.init_expr);
    bool nominal = decoder->module_->has_supertype(imm.index);

    if (!nominal) args->push_back(rtt.init_expr);
    result->init_expr = nominal ? WasmInitExpr::ArrayInitStatic(imm.index, args)
                                : WasmInitExpr::ArrayInit(imm.index, args);
  }

  void ArrayInitFromData(FullDecoder* decoder,
                         const ArrayIndexImmediate<validate>& array_imm,
                         const IndexImmediate<validate>& data_segment_imm,
                         const Value& offset_value, const Value& length_value,
                         const Value& rtt, Value* result) {
    // TODO(7748): Implement.
    UNIMPLEMENTED();
  }

  void RttCanon(FullDecoder* decoder, uint32_t type_index, Value* result) {
    result->init_expr = WasmInitExpr::RttCanon(type_index);
  }

  void DoReturn(FullDecoder* decoder, uint32_t /*drop_values*/) {
    // End decoding on "end".
    decoder->set_end(decoder->pc() + 1);
    result_ = decoder->stack_value(1)->init_expr;
  }

  WasmInitExpr result() { return result_; }

 private:
  WasmInitExpr result_;
  Zone* zone_;
};

// Appends an initializer expression encoded in {wire_bytes}, in the offset
// contained in {expr}.
void AppendInitExpr(std::ostream& os, const WasmInitExpr& expr) {
  os << "WasmInitExpr.";
  bool append_operands = false;
  switch (expr.kind()) {
    case WasmInitExpr::kNone:
      UNREACHABLE();
    case WasmInitExpr::kGlobalGet:
      os << "GlobalGet(" << expr.immediate().index;
      break;
    case WasmInitExpr::kI32Const:
      os << "I32Const(" << expr.immediate().i32_const;
      break;
    case WasmInitExpr::kI64Const:
      os << "I64Const(" << expr.immediate().i64_const;
      break;
    case WasmInitExpr::kF32Const:
      os << "F32Const(" << expr.immediate().f32_const;
      break;
    case WasmInitExpr::kF64Const:
      os << "F64Const(" << expr.immediate().f64_const;
      break;
    case WasmInitExpr::kS128Const:
      os << "S128Const([";
      for (int i = 0; i < kSimd128Size; i++) {
        os << static_cast<int>(expr.immediate().s128_const[i]);
        if (i < kSimd128Size - 1) os << ", ";
      }
      os << "]";
      break;
    case WasmInitExpr::kRefNullConst:
      os << "RefNull("
         << HeapTypeToConstantName(HeapType(expr.immediate().heap_type));
      break;
    case WasmInitExpr::kRefFuncConst:
      os << "RefFunc(" << expr.immediate().index;
      break;
    case WasmInitExpr::kStructNewWithRtt:
      os << "StructNewWithRtt(" << expr.immediate().index;
      append_operands = true;
      break;
    case WasmInitExpr::kStructNew:
      os << "StructNew(" << expr.immediate().index;
      append_operands = true;
      break;
    case WasmInitExpr::kStructNewDefaultWithRtt:
      os << "StructNewDefaultWithRtt(" << expr.immediate().index << ", ";
      AppendInitExpr(os, (*expr.operands())[0]);
      break;
    case WasmInitExpr::kStructNewDefault:
      os << "StructNewDefault(" << expr.immediate().index;
      break;
    case WasmInitExpr::kArrayInit:
      os << "ArrayInit(" << expr.immediate().index;
      append_operands = true;
      break;
    case WasmInitExpr::kArrayInitStatic:
      os << "ArrayInitStatic(" << expr.immediate().index;
      append_operands = true;
      break;
    case WasmInitExpr::kRttCanon:
      os << "RttCanon(" << expr.immediate().index;
      break;
  }

  if (append_operands) {
    os << ", [";
    for (size_t i = 0; i < expr.operands()->size(); i++) {
      AppendInitExpr(os, (*expr.operands())[i]);
      if (i < expr.operands()->size() - 1) os << ", ";
    }
    os << "]";
  }

  os << ")";
}

void DecodeAndAppendInitExpr(StdoutStream& os, Zone* zone,
                             const WasmModule* module,
                             ModuleWireBytes module_bytes,
                             ConstantExpression init, ValueType expected) {
  switch (init.kind()) {
    case ConstantExpression::kEmpty:
      UNREACHABLE();
    case ConstantExpression::kI32Const:
      AppendInitExpr(os, WasmInitExpr(init.i32_value()));
      break;
    case ConstantExpression::kRefNull:
      AppendInitExpr(os, WasmInitExpr::RefNullConst(init.repr()));
      break;
    case ConstantExpression::kRefFunc:
      AppendInitExpr(os, WasmInitExpr::RefFuncConst(init.index()));
      break;
    case ConstantExpression::kWireBytesRef: {
      WireBytesRef ref = init.wire_bytes_ref();
      auto sig = FixedSizeSignature<ValueType>::Returns(expected);
      FunctionBody body(&sig, ref.offset(), module_bytes.start() + ref.offset(),
                        module_bytes.start() + ref.end_offset());
      WasmFeatures detected;
      WasmFullDecoder<Decoder::kFullValidation, InitExprInterface,
                      kInitExpression>
          decoder(zone, module, WasmFeatures::All(), &detected, body, zone);

      decoder.DecodeFunctionBody();

      AppendInitExpr(os, decoder.interface().result());
      break;
    }
  }
}
}  // namespace

void GenerateTestCase(Isolate* isolate, ModuleWireBytes wire_bytes,
                      bool compiles) {
  constexpr bool kVerifyFunctions = false;
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(isolate);
  ModuleResult module_res = DecodeWasmModule(
      enabled_features, wire_bytes.start(), wire_bytes.end(), kVerifyFunctions,
      ModuleOrigin::kWasmOrigin, isolate->counters(),
      isolate->metrics_recorder(), v8::metrics::Recorder::ContextId::Empty(),
      DecodingMethod::kSync, GetWasmEngine()->allocator());
  CHECK_WITH_MSG(module_res.ok(), module_res.error().message().c_str());
  WasmModule* module = module_res.value().get();
  CHECK_NOT_NULL(module);

  AccountingAllocator allocator;
  Zone zone(&allocator, "init. expression zone");

  StdoutStream os;

  tzset();
  time_t current_time = time(nullptr);
  struct tm current_localtime;
#ifdef V8_OS_WIN
  localtime_s(&current_localtime, &current_time);
#else
  localtime_r(&current_time, &current_localtime);
#endif
  int year = 1900 + current_localtime.tm_year;

  os << "// Copyright " << year
     << " the V8 project authors. All rights reserved.\n"
        "// Use of this source code is governed by a BSD-style license that "
        "can be\n"
        "// found in the LICENSE file.\n"
        "\n"
        "// Flags: --wasm-staging --experimental-wasm-gc\n"
        "\n"
        "d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');\n"
        "\n"
        "const builder = new WasmModuleBuilder();\n";

  for (int i = 0; i < static_cast<int>(module->types.size()); i++) {
    if (module->has_struct(i)) {
      const StructType* struct_type = module->types[i].struct_type;
      os << "builder.addStruct([";
      int field_count = struct_type->field_count();
      for (int index = 0; index < field_count; index++) {
        os << "makeField(" << ValueTypeToConstantName(struct_type->field(index))
           << ", " << (struct_type->mutability(index) ? "true" : "false")
           << ")";
        if (index + 1 < field_count) os << ", ";
      }
      os << "]);\n";
    } else if (module->has_array(i)) {
      const ArrayType* array_type = module->types[i].array_type;
      os << "builder.addArray("
         << ValueTypeToConstantName(array_type->element_type()) << ", "
         << (array_type->mutability() ? "true" : "false") << ");\n";
    } else {
      DCHECK(module->has_signature(i));
      const FunctionSig* sig = module->types[i].function_sig;
      os << "builder.addType(makeSig(" << PrintParameters(sig) << ", "
         << PrintReturns(sig) << "));\n";
    }
  }

  for (WasmImport imported : module->import_table) {
    // TODO(wasm): Support other imports when needed.
    CHECK_EQ(kExternalFunction, imported.kind);
    auto module_name = PrintName(wire_bytes, imported.module_name);
    auto field_name = PrintName(wire_bytes, imported.field_name);
    int sig_index = module->functions[imported.index].sig_index;
    os << "builder.addImport(" << module_name << ", " << field_name << ", "
       << sig_index << " /* sig */);\n";
  }

  if (module->has_memory) {
    os << "builder.addMemory(" << module->initial_pages;
    if (module->has_maximum_pages) {
      os << ", " << module->maximum_pages;
    } else {
      os << ", undefined";
    }
    os << ", " << (module->mem_export ? "true" : "false");
    if (module->has_shared_memory) {
      os << ", true";
    }
    os << ");\n";
  }

  for (WasmDataSegment segment : module->data_segments) {
    base::Vector<const uint8_t> data = wire_bytes.module_bytes().SubVector(
        segment.source.offset(), segment.source.end_offset());
    if (segment.active) {
      // TODO(wasm): Add other expressions when needed.
      CHECK_EQ(ConstantExpression::kI32Const, segment.dest_addr.kind());
      os << "builder.addDataSegment(" << segment.dest_addr.i32_value() << ", ";
    } else {
      os << "builder.addPassiveDataSegment(";
    }
    os << "[";
    if (!data.empty()) {
      os << unsigned{data[0]};
      for (unsigned byte : data + 1) os << ", " << byte;
    }
    os << "]);\n";
  }

  for (WasmGlobal& global : module->globals) {
    os << "builder.addGlobal(" << ValueTypeToConstantName(global.type) << ", "
       << global.mutability << ", ";
    DecodeAndAppendInitExpr(os, &zone, module, wire_bytes, global.init,
                            global.type);
    os << ");\n";
  }

  Zone tmp_zone(isolate->allocator(), ZONE_NAME);

  // TODO(9495): Add support for tables with explicit initializers.
  for (const WasmTable& table : module->tables) {
    os << "builder.addTable(" << ValueTypeToConstantName(table.type) << ", "
       << table.initial_size << ", "
       << (table.has_maximum_size ? std::to_string(table.maximum_size)
                                  : "undefined")
       << ", undefined)\n";
  }
  for (const WasmElemSegment& elem_segment : module->elem_segments) {
    const char* status_str =
        elem_segment.status == WasmElemSegment::kStatusActive
            ? "Active"
            : elem_segment.status == WasmElemSegment::kStatusPassive
                  ? "Passive"
                  : "Declarative";
    os << "builder.add" << status_str << "ElementSegment(";
    if (elem_segment.status == WasmElemSegment::kStatusActive) {
      os << elem_segment.table_index << ", ";
      DecodeAndAppendInitExpr(os, &zone, module, wire_bytes,
                              elem_segment.offset, kWasmI32);
      os << ", ";
    }
    os << "[";
    for (uint32_t i = 0; i < elem_segment.entries.size(); i++) {
      if (elem_segment.element_type == WasmElemSegment::kExpressionElements) {
        DecodeAndAppendInitExpr(os, &zone, module, wire_bytes,
                                elem_segment.entries[i], elem_segment.type);
      } else {
        os << elem_segment.entries[i].index();
      }
      if (i < elem_segment.entries.size() - 1) os << ", ";
    }
    os << "], "
       << (elem_segment.element_type == WasmElemSegment::kExpressionElements
               ? ValueTypeToConstantName(elem_segment.type)
               : "undefined")
       << ");\n";
  }

  for (const WasmTag& tag : module->tags) {
    os << "builder.addTag(makeSig(" << PrintParameters(tag.ToFunctionSig())
       << ", []));\n";
  }

  for (const WasmFunction& func : module->functions) {
    if (func.imported) continue;

    base::Vector<const uint8_t> func_code = wire_bytes.GetFunctionBytes(&func);
    os << "// Generate function " << (func.func_index + 1) << " (out of "
       << module->functions.size() << ").\n";

    // Add function.
    os << "builder.addFunction(undefined, " << func.sig_index
       << " /* sig */)\n";

    // Add locals.
    BodyLocalDecls decls(&tmp_zone);
    DecodeLocalDecls(enabled_features, &decls, module, func_code.begin(),
                     func_code.end());
    if (!decls.type_list.empty()) {
      os << "  ";
      for (size_t pos = 0, count = 1, locals = decls.type_list.size();
           pos < locals; pos += count, count = 1) {
        ValueType type = decls.type_list[pos];
        while (pos + count < locals && decls.type_list[pos + count] == type) {
          ++count;
        }
        os << ".addLocals(" << ValueTypeToConstantName(type) << ", " << count
           << ")";
      }
      os << "\n";
    }

    // Add body.
    os << "  .addBodyWithEnd([\n";

    FunctionBody func_body(func.sig, func.code.offset(), func_code.begin(),
                           func_code.end());
    PrintRawWasmCode(isolate->allocator(), func_body, module, kOmitLocals);
    os << "]);\n";
  }

  for (WasmExport& exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    os << "builder.addExport(" << PrintName(wire_bytes, exp.name) << ", "
       << exp.index << ");\n";
  }

  if (compiles) {
    os << "const instance = builder.instantiate();\n"
          "print(instance.exports.main(1, 2, 3));\n";
  } else {
    os << "assertThrows(function() { builder.instantiate(); }, "
          "WebAssembly.CompileError);\n";
  }
}

void OneTimeEnableStagedWasmFeatures(v8::Isolate* isolate) {
  struct EnableStagedWasmFeatures {
    explicit EnableStagedWasmFeatures(v8::Isolate* isolate) {
#define ENABLE_STAGED_FEATURES(feat, desc, val) \
  FLAG_experimental_wasm_##feat = true;
      FOREACH_WASM_STAGING_FEATURE_FLAG(ENABLE_STAGED_FEATURES)
#undef ENABLE_STAGED_FEATURES
      isolate->InstallConditionalFeatures(isolate->GetCurrentContext());
    }
  };
  // The compiler will properly synchronize the constructor call.
  static EnableStagedWasmFeatures one_time_enable_staged_features(isolate);
}

void WasmExecutionFuzzer::FuzzWasmModule(base::Vector<const uint8_t> data,
                                         bool require_valid) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  // Strictly enforce the input size limit. Note that setting "max_len" on the
  // fuzzer target is not enough, since different fuzzers are used and not all
  // respect that limit.
  if (data.size() > max_input_size()) return;

  i::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  i_isolate->clear_pending_exception();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // We explicitly enable staged WebAssembly features here to increase fuzzer
  // coverage. For libfuzzer fuzzers it is not possible that the fuzzer enables
  // the flag by itself.
  OneTimeEnableStagedWasmFeatures(isolate);

  v8::TryCatch try_catch(isolate);
  HandleScope scope(i_isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);

  // The first byte specifies some internal configuration, like which function
  // is compiled with with compiler, and other flags.
  uint8_t configuration_byte = data.empty() ? 0 : data[0];
  if (!data.empty()) data += 1;

  // Derive the compiler configuration for the first four functions from the
  // configuration byte, to choose for each function between:
  // 0: TurboFan
  // 1: Liftoff
  // 2: Liftoff for debugging
  uint8_t tier_mask = 0;
  uint8_t debug_mask = 0;
  for (int i = 0; i < 4; ++i, configuration_byte /= 3) {
    int compiler_config = configuration_byte % 3;
    tier_mask |= (compiler_config == 0) << i;
    debug_mask |= (compiler_config == 2) << i;
  }
  // Note: After dividing by 3 for 4 times, configuration_byte is within [0, 3].

// Control whether Liftoff or the interpreter will be used as the reference
// tier.
// TODO(thibaudm): Port nondeterminism detection to arm.
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_X86) || \
    defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_ARM)
  bool liftoff_as_reference = configuration_byte & 1;
#else
  bool liftoff_as_reference = false;
#endif
  FlagScope<bool> turbo_mid_tier_regalloc(&FLAG_turbo_force_mid_tier_regalloc,
                                          configuration_byte == 0);

  if (!GenerateModule(i_isolate, &zone, data, &buffer, liftoff_as_reference)) {
    return;
  }

  testing::SetupIsolateForWasmModule(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());

  if (require_valid && FLAG_wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, true);
  }

  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  MaybeHandle<WasmModuleObject> compiled_module;
  {
    // Explicitly enable Liftoff, disable tiering and set the tier_mask. This
    // way, we deterministically test a combination of Liftoff and Turbofan.
    FlagScope<bool> liftoff(&FLAG_liftoff, true);
    FlagScope<bool> no_tier_up(&FLAG_wasm_tier_up, false);
    FlagScope<int> tier_mask_scope(&FLAG_wasm_tier_mask_for_testing, tier_mask);
    FlagScope<int> debug_mask_scope(&FLAG_wasm_debug_mask_for_testing,
                                    debug_mask);
    compiled_module = GetWasmEngine()->SyncCompile(
        i_isolate, enabled_features, &interpreter_thrower, wire_bytes);
  }
  bool compiles = !compiled_module.is_null();
  if (!require_valid && FLAG_wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, compiles);
  }

  std::string error_message;
  bool result = GetWasmEngine()->SyncValidate(i_isolate, enabled_features,
                                              wire_bytes, &error_message);

  CHECK_EQ(compiles, result);
  CHECK_WITH_MSG(
      !require_valid || result,
      ("Generated module should validate, but got: " + error_message).c_str());

  if (!compiles) return;

  int32_t max_steps = 16 * 1024;
  int32_t nondeterminism = false;
  Handle<WasmModuleObject> module_ref;
  if (liftoff_as_reference) {
    module_ref = CompileReferenceModule(&zone, i_isolate, wire_bytes,
                                        &interpreter_thrower, &max_steps,
                                        &nondeterminism);
  }
  InterpretAndExecuteModule(i_isolate, compiled_module.ToHandleChecked(),
                            module_ref, &max_steps, &nondeterminism);
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
