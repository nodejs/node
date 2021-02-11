// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/wasm/wasm-run-utils.h"

#include "src/codegen/assembler-inl.h"
#include "src/diagnostics/code-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

TestingModuleBuilder::TestingModuleBuilder(
    Zone* zone, ManuallyImportedJSFunction* maybe_import,
    TestExecutionTier tier, RuntimeExceptionSupport exception_support,
    LowerSimd lower_simd)
    : test_module_(std::make_shared<WasmModule>()),
      test_module_ptr_(test_module_.get()),
      isolate_(CcTest::InitIsolateOnce()),
      enabled_features_(WasmFeatures::FromIsolate(isolate_)),
      execution_tier_(tier),
      runtime_exception_support_(exception_support),
      lower_simd_(lower_simd) {
  WasmJs::Install(isolate_, true);
  test_module_->untagged_globals_buffer_size = kMaxGlobalsSize;
  memset(globals_data_, 0, sizeof(globals_data_));

  uint32_t maybe_import_index = 0;
  if (maybe_import) {
    // Manually add an imported function before any other functions.
    // This must happen before the instance object is created, since the
    // instance object allocates import entries.
    maybe_import_index = AddFunction(maybe_import->sig, nullptr, kImport);
    DCHECK_EQ(0, maybe_import_index);
  }

  instance_object_ = InitInstanceObject();
  Handle<FixedArray> tables(isolate_->factory()->NewFixedArray(0));
  instance_object_->set_tables(*tables);

  if (maybe_import) {
    // Manually compile an import wrapper and insert it into the instance.
    CodeSpaceMemoryModificationScope modification_scope(isolate_->heap());
    auto resolved = compiler::ResolveWasmImportCall(
        maybe_import->js_function, maybe_import->sig,
        instance_object_->module(), enabled_features_);
    compiler::WasmImportCallKind kind = resolved.first;
    Handle<JSReceiver> callable = resolved.second;
    WasmImportWrapperCache::ModificationScope cache_scope(
        native_module_->import_wrapper_cache());
    WasmImportWrapperCache::CacheKey key(
        kind, maybe_import->sig,
        static_cast<int>(maybe_import->sig->parameter_count()));
    auto import_wrapper = cache_scope[key];
    if (import_wrapper == nullptr) {
      import_wrapper = CompileImportWrapper(
          isolate_->wasm_engine(), native_module_, isolate_->counters(), kind,
          maybe_import->sig,
          static_cast<int>(maybe_import->sig->parameter_count()), &cache_scope);
    }

    ImportedFunctionEntry(instance_object_, maybe_import_index)
        .SetWasmToJs(isolate_, callable, import_wrapper);
  }

  if (tier == TestExecutionTier::kInterpreter) {
    interpreter_ = std::make_unique<WasmInterpreter>(
        isolate_, test_module_ptr_,
        ModuleWireBytes{native_module_->wire_bytes()}, instance_object_);
  }
}

TestingModuleBuilder::~TestingModuleBuilder() {
  // When the native module dies and is erased from the cache, it is expected to
  // have either valid bytes or no bytes at all.
  native_module_->SetWireBytes({});
}

byte* TestingModuleBuilder::AddMemory(uint32_t size, SharedFlag shared) {
  CHECK(!test_module_->has_memory);
  CHECK_NULL(mem_start_);
  CHECK_EQ(0, mem_size_);
  DCHECK(!instance_object_->has_memory_object());
  uint32_t initial_pages = RoundUp(size, kWasmPageSize) / kWasmPageSize;
  uint32_t maximum_pages = (test_module_->maximum_pages != 0)
                               ? test_module_->maximum_pages
                               : initial_pages;
  test_module_->has_memory = true;

  // Create the WasmMemoryObject.
  Handle<WasmMemoryObject> memory_object =
      WasmMemoryObject::New(isolate_, initial_pages, maximum_pages, shared)
          .ToHandleChecked();
  instance_object_->set_memory_object(*memory_object);

  mem_start_ =
      reinterpret_cast<byte*>(memory_object->array_buffer().backing_store());
  mem_size_ = size;
  CHECK(size == 0 || mem_start_);

  WasmMemoryObject::AddInstance(isolate_, memory_object, instance_object_);
  // TODO(wasm): Delete the following two lines when test-run-wasm will use a
  // multiple of kPageSize as memory size. At the moment, the effect of these
  // two lines is used to shrink the memory for testing purposes.
  instance_object_->SetRawMemory(mem_start_, mem_size_);
  return mem_start_;
}

uint32_t TestingModuleBuilder::AddFunction(const FunctionSig* sig,
                                           const char* name,
                                           FunctionType type) {
  if (test_module_->functions.size() == 0) {
    // TODO(titzer): Reserving space here to avoid the underlying WasmFunction
    // structs from moving.
    test_module_->functions.reserve(kMaxFunctions);
  }
  uint32_t index = static_cast<uint32_t>(test_module_->functions.size());
  test_module_->functions.push_back({sig,      // sig
                                     index,    // func_index
                                     0,        // sig_index
                                     {0, 0},   // code
                                     false,    // imported
                                     false,    // exported
                                     false});  // declared
  if (type == kImport) {
    DCHECK_EQ(0, test_module_->num_declared_functions);
    ++test_module_->num_imported_functions;
    test_module_->functions.back().imported = true;
  } else {
    ++test_module_->num_declared_functions;
  }
  DCHECK_EQ(test_module_->functions.size(),
            test_module_->num_imported_functions +
                test_module_->num_declared_functions);
  if (name) {
    Vector<const byte> name_vec = Vector<const byte>::cast(CStrVector(name));
    test_module_->lazily_generated_names.AddForTesting(
        index, {AddBytes(name_vec), static_cast<uint32_t>(name_vec.length())});
  }
  if (interpreter_) {
    interpreter_->AddFunctionForTesting(&test_module_->functions.back());
  }
  DCHECK_LT(index, kMaxFunctions);  // limited for testing.
  return index;
}

void TestingModuleBuilder::FreezeSignatureMapAndInitializeWrapperCache() {
  if (test_module_->signature_map.is_frozen()) return;
  test_module_->signature_map.Freeze();
  size_t max_num_sigs = MaxNumExportWrappers(test_module_.get());
  Handle<FixedArray> export_wrappers =
      isolate_->factory()->NewFixedArray(static_cast<int>(max_num_sigs));
  instance_object_->module_object().set_export_wrappers(*export_wrappers);
}

Handle<JSFunction> TestingModuleBuilder::WrapCode(uint32_t index) {
  CHECK(!interpreter_);
  FreezeSignatureMapAndInitializeWrapperCache();
  SetExecutable();
  return WasmInstanceObject::GetOrCreateWasmExternalFunction(
      isolate_, instance_object(), index);
}

void TestingModuleBuilder::AddIndirectFunctionTable(
    const uint16_t* function_indexes, uint32_t table_size,
    ValueType table_type) {
  Handle<WasmInstanceObject> instance = instance_object();
  uint32_t table_index = static_cast<uint32_t>(test_module_->tables.size());
  test_module_->tables.emplace_back();
  WasmTable& table = test_module_->tables.back();
  table.initial_size = table_size;
  table.maximum_size = table_size;
  table.has_maximum_size = true;
  table.type = table_type;

  {
    // Allocate the indirect function table.
    Handle<FixedArray> old_tables =
        table_index == 0
            ? isolate_->factory()->empty_fixed_array()
            : handle(instance_object_->indirect_function_tables(), isolate_);
    Handle<FixedArray> new_tables =
        isolate_->factory()->CopyFixedArrayAndGrow(old_tables, 1);
    Handle<WasmIndirectFunctionTable> table_obj =
        WasmIndirectFunctionTable::New(isolate_, table.initial_size);
    new_tables->set(table_index, *table_obj);
    instance_object_->set_indirect_function_tables(*new_tables);
  }

  WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
      instance_object(), table_index, table_size);
  Handle<WasmTableObject> table_obj =
      WasmTableObject::New(isolate_, instance, table.type, table.initial_size,
                           table.has_maximum_size, table.maximum_size, nullptr);

  WasmTableObject::AddDispatchTable(isolate_, table_obj, instance_object_,
                                    table_index);

  if (function_indexes) {
    for (uint32_t i = 0; i < table_size; ++i) {
      WasmFunction& function = test_module_->functions[function_indexes[i]];
      int sig_id = test_module_->signature_map.Find(*function.sig);
      IndirectFunctionTableEntry(instance, table_index, i)
          .Set(sig_id, instance, function.func_index);
      WasmTableObject::SetFunctionTablePlaceholder(
          isolate_, table_obj, i, instance_object_, function_indexes[i]);
    }
  }

  Handle<FixedArray> old_tables(instance_object_->tables(), isolate_);
  Handle<FixedArray> new_tables =
      isolate_->factory()->CopyFixedArrayAndGrow(old_tables, 1);
  new_tables->set(old_tables->length(), *table_obj);
  instance_object_->set_tables(*new_tables);
}

uint32_t TestingModuleBuilder::AddBytes(Vector<const byte> bytes) {
  Vector<const uint8_t> old_bytes = native_module_->wire_bytes();
  uint32_t old_size = static_cast<uint32_t>(old_bytes.size());
  // Avoid placing strings at offset 0, this might be interpreted as "not
  // set", e.g. for function names.
  uint32_t bytes_offset = old_size ? old_size : 1;
  size_t new_size = bytes_offset + bytes.size();
  OwnedVector<uint8_t> new_bytes = OwnedVector<uint8_t>::New(new_size);
  if (old_size > 0) {
    memcpy(new_bytes.start(), old_bytes.begin(), old_size);
  } else {
    // Set the unused byte. It is never decoded, but the bytes are used as the
    // key in the native module cache.
    new_bytes[0] = 0;
  }
  memcpy(new_bytes.start() + bytes_offset, bytes.begin(), bytes.length());
  native_module_->SetWireBytes(std::move(new_bytes));
  return bytes_offset;
}

uint32_t TestingModuleBuilder::AddException(const FunctionSig* sig) {
  DCHECK_EQ(0, sig->return_count());
  uint32_t index = static_cast<uint32_t>(test_module_->exceptions.size());
  test_module_->exceptions.push_back(WasmException{sig});
  Handle<WasmExceptionTag> tag = WasmExceptionTag::New(isolate_, index);
  Handle<FixedArray> table(instance_object_->exceptions_table(), isolate_);
  table = isolate_->factory()->CopyFixedArrayAndGrow(table, 1);
  instance_object_->set_exceptions_table(*table);
  table->set(index, *tag);
  return index;
}

uint32_t TestingModuleBuilder::AddPassiveDataSegment(Vector<const byte> bytes) {
  uint32_t index = static_cast<uint32_t>(test_module_->data_segments.size());
  DCHECK_EQ(index, test_module_->data_segments.size());
  DCHECK_EQ(index, data_segment_starts_.size());
  DCHECK_EQ(index, data_segment_sizes_.size());

  // Add a passive data segment. This isn't used by function compilation, but
  // but it keeps the index in sync. The data segment's source will not be
  // correct, since we don't store data in the module wire bytes.
  test_module_->data_segments.emplace_back();

  // The num_declared_data_segments (from the DataCount section) is used
  // to validate the segment index, during function compilation.
  test_module_->num_declared_data_segments = index + 1;

  Address old_data_address =
      reinterpret_cast<Address>(data_segment_data_.data());
  size_t old_data_size = data_segment_data_.size();
  data_segment_data_.resize(old_data_size + bytes.length());
  Address new_data_address =
      reinterpret_cast<Address>(data_segment_data_.data());

  memcpy(data_segment_data_.data() + old_data_size, bytes.begin(),
         bytes.length());

  // The data_segment_data_ offset may have moved, so update all the starts.
  for (Address& start : data_segment_starts_) {
    start += new_data_address - old_data_address;
  }
  data_segment_starts_.push_back(new_data_address + old_data_size);
  data_segment_sizes_.push_back(bytes.length());

  // The vector pointers may have moved, so update the instance object.
  instance_object_->set_data_segment_starts(data_segment_starts_.data());
  instance_object_->set_data_segment_sizes(data_segment_sizes_.data());
  return index;
}

uint32_t TestingModuleBuilder::AddPassiveElementSegment(
    const std::vector<uint32_t>& entries) {
  uint32_t index = static_cast<uint32_t>(test_module_->elem_segments.size());
  DCHECK_EQ(index, dropped_elem_segments_.size());

  test_module_->elem_segments.emplace_back(false);
  auto& elem_segment = test_module_->elem_segments.back();
  elem_segment.entries = entries;

  // The vector pointers may have moved, so update the instance object.
  dropped_elem_segments_.push_back(0);
  instance_object_->set_dropped_elem_segments(dropped_elem_segments_.data());
  return index;
}

CompilationEnv TestingModuleBuilder::CreateCompilationEnv() {
  // This is a hack so we don't need to call
  // trap_handler::IsTrapHandlerEnabled().
  const bool is_trap_handler_enabled =
      V8_TRAP_HANDLER_SUPPORTED && i::FLAG_wasm_trap_handler;
  return {test_module_ptr_,
          is_trap_handler_enabled ? kUseTrapHandler : kNoTrapHandler,
          runtime_exception_support_, enabled_features_, lower_simd()};
}

const WasmGlobal* TestingModuleBuilder::AddGlobal(ValueType type) {
  byte size = type.element_size_bytes();
  global_offset = (global_offset + size - 1) & ~(size - 1);  // align
  test_module_->globals.push_back(
      {type, true, WasmInitExpr(), {global_offset}, false, false});
  global_offset += size;
  // limit number of globals.
  CHECK_LT(global_offset, kMaxGlobalsSize);
  return &test_module_->globals.back();
}

Handle<WasmInstanceObject> TestingModuleBuilder::InitInstanceObject() {
  const bool kUsesLiftoff = true;
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(test_module_.get(),
                                                          kUsesLiftoff);
  auto native_module = isolate_->wasm_engine()->NewNativeModule(
      isolate_, enabled_features_, test_module_, code_size_estimate);
  native_module->SetWireBytes(OwnedVector<const uint8_t>());
  Handle<Script> script =
      isolate_->wasm_engine()->GetOrCreateScript(isolate_, native_module);

  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate_, std::move(native_module), script);
  // This method is called when we initialize TestEnvironment. We don't
  // have a memory yet, so we won't create it here. We'll update the
  // interpreter when we get a memory. We do have globals, though.
  native_module_ = module_object->native_module();
  native_module_->ReserveCodeTableForTesting(kMaxFunctions);

  auto instance = WasmInstanceObject::New(isolate_, module_object);
  instance->set_exceptions_table(*isolate_->factory()->empty_fixed_array());
  instance->set_globals_start(globals_data_);
  return instance;
}

void TestBuildingGraphWithBuilder(compiler::WasmGraphBuilder* builder,
                                  Zone* zone, const FunctionSig* sig,
                                  const byte* start, const byte* end) {
  WasmFeatures unused_detected_features;
  FunctionBody body(sig, 0, start, end);
  DecodeResult result =
      BuildTFGraph(zone->allocator(), WasmFeatures::All(), nullptr, builder,
                   &unused_detected_features, body, nullptr);
  if (result.failed()) {
#ifdef DEBUG
    if (!FLAG_trace_wasm_decoder) {
      // Retry the compilation with the tracing flag on, to help in debugging.
      FLAG_trace_wasm_decoder = true;
      result = BuildTFGraph(zone->allocator(), WasmFeatures::All(), nullptr,
                            builder, &unused_detected_features, body, nullptr);
    }
#endif

    FATAL("Verification failed; pc = +%x, msg = %s", result.error().offset(),
          result.error().message().c_str());
  }
  builder->LowerInt64(compiler::WasmGraphBuilder::kCalledFromWasm);
  if (!CpuFeatures::SupportsWasmSimd128()) {
    builder->SimdScalarLoweringForTesting();
  }
}

void TestBuildingGraph(Zone* zone, compiler::JSGraph* jsgraph,
                       CompilationEnv* module, const FunctionSig* sig,
                       compiler::SourcePositionTable* source_position_table,
                       const byte* start, const byte* end) {
  compiler::WasmGraphBuilder builder(module, zone, jsgraph, sig,
                                     source_position_table);
  TestBuildingGraphWithBuilder(&builder, zone, sig, start, end);
}

WasmFunctionWrapper::WasmFunctionWrapper(Zone* zone, int num_params)
    : GraphAndBuilders(zone),
      inner_code_node_(nullptr),
      context_address_(nullptr),
      signature_(nullptr) {
  // One additional parameter for the pointer to the return value memory.
  Signature<MachineType>::Builder sig_builder(zone, 1, num_params + 1);

  sig_builder.AddReturn(MachineType::Int32());
  for (int i = 0; i < num_params + 1; i++) {
    sig_builder.AddParam(MachineType::Pointer());
  }
  signature_ = sig_builder.Build();
}

void WasmFunctionWrapper::Init(CallDescriptor* call_descriptor,
                               MachineType return_type,
                               Vector<MachineType> param_types) {
  DCHECK_NOT_NULL(call_descriptor);
  DCHECK_EQ(signature_->parameter_count(), param_types.length() + 1);

  // Create the TF graph for the wrapper.

  // Function, context_address, effect, and control.
  Node** parameters = zone()->NewArray<Node*>(param_types.length() + 4);
  int start_value_output_count =
      static_cast<int>(signature_->parameter_count()) + 1;
  graph()->SetStart(
      graph()->NewNode(common()->Start(start_value_output_count)));
  Node* effect = graph()->start();
  int parameter_count = 0;

  // Dummy node which gets replaced in SetInnerCode.
  inner_code_node_ = graph()->NewNode(common()->Int32Constant(0));
  parameters[parameter_count++] = inner_code_node_;

  // Dummy node that gets replaced in SetContextAddress.
  context_address_ = graph()->NewNode(IntPtrConstant(0));
  parameters[parameter_count++] = context_address_;

  int param_idx = 0;
  for (MachineType t : param_types) {
    DCHECK_NE(MachineType::None(), t);
    parameters[parameter_count] = graph()->NewNode(
        machine()->Load(t),
        graph()->NewNode(common()->Parameter(param_idx++), graph()->start()),
        graph()->NewNode(common()->Int32Constant(0)), effect, graph()->start());
    effect = parameters[parameter_count++];
  }

  parameters[parameter_count++] = effect;
  parameters[parameter_count++] = graph()->start();
  Node* call = graph()->NewNode(common()->Call(call_descriptor),
                                parameter_count, parameters);

  if (!return_type.IsNone()) {
    effect = graph()->NewNode(
        machine()->Store(compiler::StoreRepresentation(
            return_type.representation(),
            compiler::WriteBarrierKind::kNoWriteBarrier)),
        graph()->NewNode(common()->Parameter(param_types.length()),
                         graph()->start()),
        graph()->NewNode(common()->Int32Constant(0)), call, effect,
        graph()->start());
  }
  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* r = graph()->NewNode(
      common()->Return(), zero,
      graph()->NewNode(common()->Int32Constant(WASM_WRAPPER_RETURN_VALUE)),
      effect, graph()->start());
  graph()->SetEnd(graph()->NewNode(common()->End(1), r));
}

Handle<Code> WasmFunctionWrapper::GetWrapperCode() {
  Handle<Code> code;
  if (!code_.ToHandle(&code)) {
    Isolate* isolate = CcTest::InitIsolateOnce();

    auto call_descriptor = compiler::Linkage::GetSimplifiedCDescriptor(
        zone(), signature_, CallDescriptor::kInitializeRootRegister);

    if (kSystemPointerSize == 4) {
      size_t num_params = signature_->parameter_count();
      // One additional parameter for the pointer of the return value.
      Signature<MachineRepresentation>::Builder rep_builder(zone(), 1,
                                                            num_params + 1);

      rep_builder.AddReturn(MachineRepresentation::kWord32);
      for (size_t i = 0; i < num_params + 1; i++) {
        rep_builder.AddParam(MachineRepresentation::kWord32);
      }
      compiler::Int64Lowering r(graph(), machine(), common(), zone(),
                                rep_builder.Build());
      r.LowerGraph();
    }

    OptimizedCompilationInfo info(ArrayVector("testing"), graph()->zone(),
                                  CodeKind::C_WASM_ENTRY);
    code_ = compiler::Pipeline::GenerateCodeForTesting(
        &info, isolate, call_descriptor, graph(),
        AssemblerOptions::Default(isolate));
    code = code_.ToHandleChecked();
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code) {
      CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
      OFStream os(tracing_scope.file());

      code->Disassemble("wasm wrapper", os, isolate);
    }
#endif
  }

  return code;
}

// This struct is just a type tag for Zone::NewArray<T>(size_t) call.
struct WasmFunctionCompilerBuffer {};

void WasmFunctionCompiler::Build(const byte* start, const byte* end) {
  size_t locals_size = local_decls.Size();
  size_t total_size = end - start + locals_size + 1;
  byte* buffer = zone()->NewArray<byte, WasmFunctionCompilerBuffer>(total_size);
  // Prepend the local decls to the code.
  local_decls.Emit(buffer);
  // Emit the code.
  memcpy(buffer + locals_size, start, end - start);
  // Append an extra end opcode.
  buffer[total_size - 1] = kExprEnd;

  start = buffer;
  end = buffer + total_size;

  CHECK_GE(kMaxInt, end - start);
  int len = static_cast<int>(end - start);
  function_->code = {builder_->AddBytes(Vector<const byte>(start, len)),
                     static_cast<uint32_t>(len)};

  if (interpreter_) {
    // Add the code to the interpreter; do not generate compiled code.
    interpreter_->SetFunctionCodeForTesting(function_, start, end);
    return;
  }

  Vector<const uint8_t> wire_bytes = builder_->instance_object()
                                         ->module_object()
                                         .native_module()
                                         ->wire_bytes();

  CompilationEnv env = builder_->CreateCompilationEnv();
  ScopedVector<uint8_t> func_wire_bytes(function_->code.length());
  memcpy(func_wire_bytes.begin(), wire_bytes.begin() + function_->code.offset(),
         func_wire_bytes.length());

  FunctionBody func_body{function_->sig, function_->code.offset(),
                         func_wire_bytes.begin(), func_wire_bytes.end()};
  NativeModule* native_module =
      builder_->instance_object()->module_object().native_module();
  ForDebugging for_debugging =
      native_module->IsTieredDown() ? kForDebugging : kNoDebugging;
  WasmCompilationUnit unit(function_->func_index, builder_->execution_tier(),
                           for_debugging);
  WasmFeatures unused_detected_features;
  WasmCompilationResult result = unit.ExecuteCompilation(
      isolate()->wasm_engine(), &env,
      native_module->compilation_state()->GetWireBytesStorage(),
      isolate()->counters(), &unused_detected_features);
  WasmCode* code = native_module->PublishCode(
      native_module->AddCompiledCode(std::move(result)));
  DCHECK_NOT_NULL(code);
  if (WasmCode::ShouldBeLogged(isolate())) code->LogCode(isolate());
}

WasmFunctionCompiler::WasmFunctionCompiler(Zone* zone, const FunctionSig* sig,
                                           TestingModuleBuilder* builder,
                                           const char* name)
    : GraphAndBuilders(zone),
      jsgraph(builder->isolate(), this->graph(), this->common(), nullptr,
              nullptr, this->machine()),
      sig(sig),
      descriptor_(nullptr),
      builder_(builder),
      local_decls(zone, sig),
      source_position_table_(this->graph()),
      interpreter_(builder->interpreter()) {
  // Get a new function from the testing module.
  int index = builder->AddFunction(sig, name, TestingModuleBuilder::kWasm);
  function_ = builder_->GetFunctionAt(index);
}

WasmFunctionCompiler::~WasmFunctionCompiler() = default;

/* static */
FunctionSig* WasmRunnerBase::CreateSig(Zone* zone, MachineType return_type,
                                       Vector<MachineType> param_types) {
  int return_count = return_type.IsNone() ? 0 : 1;
  int param_count = param_types.length();

  // Allocate storage array in zone.
  ValueType* sig_types = zone->NewArray<ValueType>(return_count + param_count);

  // Convert machine types to local types, and check that there are no
  // MachineType::None()'s in the parameters.
  int idx = 0;
  if (return_count) sig_types[idx++] = ValueType::For(return_type);
  for (MachineType param : param_types) {
    CHECK_NE(MachineType::None(), param);
    sig_types[idx++] = ValueType::For(param);
  }
  return zone->New<FunctionSig>(return_count, param_count, sig_types);
}

// static
bool WasmRunnerBase::trap_happened;

}  // namespace wasm
}  // namespace internal
}  // namespace v8
