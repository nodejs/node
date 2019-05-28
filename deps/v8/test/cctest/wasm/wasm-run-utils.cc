// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/wasm/wasm-run-utils.h"

#include "src/assembler-inl.h"
#include "src/code-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

TestingModuleBuilder::TestingModuleBuilder(
    Zone* zone, ManuallyImportedJSFunction* maybe_import, ExecutionTier tier,
    RuntimeExceptionSupport exception_support, LowerSimd lower_simd)
    : test_module_(std::make_shared<WasmModule>()),
      test_module_ptr_(test_module_.get()),
      isolate_(CcTest::InitIsolateOnce()),
      enabled_features_(WasmFeaturesFromIsolate(isolate_)),
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
    auto kind = compiler::GetWasmImportCallKind(maybe_import->js_function,
                                                maybe_import->sig, false);
    auto import_wrapper = native_module_->import_wrapper_cache()->GetOrCompile(
        isolate_->wasm_engine(), isolate_->counters(), kind, maybe_import->sig);

    ImportedFunctionEntry(instance_object_, maybe_import_index)
        .SetWasmToJs(isolate_, maybe_import->js_function, import_wrapper);
  }

  if (tier == ExecutionTier::kInterpreter) {
    interpreter_ = WasmDebugInfo::SetupForTesting(instance_object_);
  }
}

byte* TestingModuleBuilder::AddMemory(uint32_t size, SharedFlag shared) {
  CHECK(!test_module_->has_memory);
  CHECK_NULL(mem_start_);
  CHECK_EQ(0, mem_size_);
  DCHECK(!instance_object_->has_memory_object());
  DCHECK_IMPLIES(test_module_->origin == kWasmOrigin,
                 size % kWasmPageSize == 0);
  test_module_->has_memory = true;
  uint32_t max_size =
      (test_module_->maximum_pages != 0) ? test_module_->maximum_pages : size;
  uint32_t alloc_size = RoundUp(size, kWasmPageSize);
  Handle<JSArrayBuffer> new_buffer;
  if (shared == SharedFlag::kShared) {
    CHECK(NewSharedArrayBuffer(isolate_, alloc_size, max_size)
              .ToHandle(&new_buffer));
  } else {
    CHECK(NewArrayBuffer(isolate_, alloc_size).ToHandle(&new_buffer));
  }
  CHECK(!new_buffer.is_null());
  mem_start_ = reinterpret_cast<byte*>(new_buffer->backing_store());
  mem_size_ = size;
  CHECK(size == 0 || mem_start_);
  memset(mem_start_, 0, size);

  // Create the WasmMemoryObject.
  Handle<WasmMemoryObject> memory_object =
      WasmMemoryObject::New(isolate_, new_buffer, max_size);
  instance_object_->set_memory_object(*memory_object);
  WasmMemoryObject::AddInstance(isolate_, memory_object, instance_object_);
  // TODO(wasm): Delete the following two lines when test-run-wasm will use a
  // multiple of kPageSize as memory size. At the moment, the effect of these
  // two lines is used to shrink the memory for testing purposes.
  instance_object_->SetRawMemory(mem_start_, mem_size_);
  return mem_start_;
}

uint32_t TestingModuleBuilder::AddFunction(FunctionSig* sig, const char* name,
                                           FunctionType type) {
  if (test_module_->functions.size() == 0) {
    // TODO(titzer): Reserving space here to avoid the underlying WasmFunction
    // structs from moving.
    test_module_->functions.reserve(kMaxFunctions);
  }
  uint32_t index = static_cast<uint32_t>(test_module_->functions.size());
  test_module_->functions.push_back({sig, index, 0, {0, 0}, false, false});
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
    test_module_->AddFunctionNameForTesting(
        index, {AddBytes(name_vec), static_cast<uint32_t>(name_vec.length())});
  }
  if (interpreter_) {
    interpreter_->AddFunctionForTesting(&test_module_->functions.back());
    // Patch the jump table to call the interpreter for this function.
    wasm::WasmCompilationResult result = compiler::CompileWasmInterpreterEntry(
        isolate_->wasm_engine(), native_module_->enabled_features(), index,
        sig);
    std::unique_ptr<wasm::WasmCode> code = native_module_->AddCode(
        index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots, std::move(result.protected_instructions),
        std::move(result.source_positions), wasm::WasmCode::kInterpreterEntry,
        wasm::ExecutionTier::kInterpreter);
    native_module_->PublishCode(std::move(code));
  }
  DCHECK_LT(index, kMaxFunctions);  // limited for testing.
  return index;
}

Handle<JSFunction> TestingModuleBuilder::WrapCode(uint32_t index) {
  SetExecutable();
  FunctionSig* sig = test_module_->functions[index].sig;
  MaybeHandle<Code> maybe_ret_code =
      compiler::CompileJSToWasmWrapper(isolate_, sig, false);
  Handle<Code> ret_code = maybe_ret_code.ToHandleChecked();
  Handle<JSFunction> ret = WasmExportedFunction::New(
      isolate_, instance_object(), MaybeHandle<String>(),
      static_cast<int>(index), static_cast<int>(sig->parameter_count()),
      ret_code);

  // Add reference to the exported wrapper code.
  Handle<WasmModuleObject> module_object(instance_object()->module_object(),
                                         isolate_);
  Handle<FixedArray> old_arr(module_object->export_wrappers(), isolate_);
  Handle<FixedArray> new_arr =
      isolate_->factory()->NewFixedArray(old_arr->length() + 1);
  old_arr->CopyTo(0, *new_arr, 0, old_arr->length());
  new_arr->set(old_arr->length(), *ret_code);
  module_object->set_export_wrappers(*new_arr);
  return ret;
}

void TestingModuleBuilder::AddIndirectFunctionTable(
    const uint16_t* function_indexes, uint32_t table_size) {
  auto instance = instance_object();
  uint32_t table_index = static_cast<uint32_t>(test_module_->tables.size());
  test_module_->tables.emplace_back();
  WasmTable& table = test_module_->tables.back();
  table.initial_size = table_size;
  table.maximum_size = table_size;
  table.has_maximum_size = true;
  table.type = kWasmAnyFunc;
  WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
      instance_object(), table_size);
  Handle<WasmTableObject> table_obj =
      WasmTableObject::New(isolate_, table.type, table.initial_size,
                           table.has_maximum_size, table.maximum_size, nullptr);

  WasmTableObject::AddDispatchTable(isolate_, table_obj, instance_object_,
                                    table_index);

  if (function_indexes) {
    for (uint32_t i = 0; i < table_size; ++i) {
      WasmFunction& function = test_module_->functions[function_indexes[i]];
      int sig_id = test_module_->signature_map.Find(*function.sig);
      IndirectFunctionTableEntry(instance, i)
          .Set(sig_id, instance, function.func_index);
      WasmTableObject::SetFunctionTablePlaceholder(
          isolate_, table_obj, i, instance_object_, function_indexes[i]);
    }
  }

  Handle<FixedArray> old_tables(instance_object_->tables(), isolate_);
  Handle<FixedArray> new_tables = isolate_->factory()->CopyFixedArrayAndGrow(
      old_tables, old_tables->length() + 1);
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
    memcpy(new_bytes.start(), old_bytes.start(), old_size);
  }
  memcpy(new_bytes.start() + bytes_offset, bytes.start(), bytes.length());
  native_module_->SetWireBytes(std::move(new_bytes));
  return bytes_offset;
}

uint32_t TestingModuleBuilder::AddException(FunctionSig* sig) {
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
  DCHECK_EQ(index, dropped_data_segments_.size());

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

  memcpy(data_segment_data_.data() + old_data_size, bytes.start(),
         bytes.length());

  // The data_segment_data_ offset may have moved, so update all the starts.
  for (Address& start : data_segment_starts_) {
    start += new_data_address - old_data_address;
  }
  data_segment_starts_.push_back(new_data_address + old_data_size);
  data_segment_sizes_.push_back(bytes.length());
  dropped_data_segments_.push_back(0);

  // The vector pointers may have moved, so update the instance object.
  instance_object_->set_data_segment_starts(data_segment_starts_.data());
  instance_object_->set_data_segment_sizes(data_segment_sizes_.data());
  instance_object_->set_dropped_data_segments(dropped_data_segments_.data());
  return index;
}

uint32_t TestingModuleBuilder::AddPassiveElementSegment(
    const std::vector<uint32_t>& entries) {
  uint32_t index = static_cast<uint32_t>(test_module_->elem_segments.size());
  DCHECK_EQ(index, dropped_elem_segments_.size());

  test_module_->elem_segments.emplace_back();
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
  byte size = ValueTypes::MemSize(ValueTypes::MachineTypeFor(type));
  global_offset = (global_offset + size - 1) & ~(size - 1);  // align
  test_module_->globals.push_back(
      {type, true, WasmInitExpr(), {global_offset}, false, false});
  global_offset += size;
  // limit number of globals.
  CHECK_LT(global_offset, kMaxGlobalsSize);
  return &test_module_->globals.back();
}

Handle<WasmInstanceObject> TestingModuleBuilder::InitInstanceObject() {
  Handle<Script> script =
      isolate_->factory()->NewScript(isolate_->factory()->empty_string());
  script->set_type(Script::TYPE_WASM);
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate_, enabled_features_, test_module_, {},
                            script, Handle<ByteArray>::null());
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
                                  Zone* zone, FunctionSig* sig,
                                  const byte* start, const byte* end) {
  WasmFeatures unused_detected_features;
  FunctionBody body(sig, 0, start, end);
  DecodeResult result =
      BuildTFGraph(zone->allocator(), kAllWasmFeatures, nullptr, builder,
                   &unused_detected_features, body, nullptr);
  if (result.failed()) {
#ifdef DEBUG
    if (!FLAG_trace_wasm_decoder) {
      // Retry the compilation with the tracing flag on, to help in debugging.
      FLAG_trace_wasm_decoder = true;
      result = BuildTFGraph(zone->allocator(), kAllWasmFeatures, nullptr,
                            builder, &unused_detected_features, body, nullptr);
    }
#endif

    FATAL("Verification failed; pc = +%x, msg = %s", result.error().offset(),
          result.error().message().c_str());
  }
  builder->LowerInt64();
  if (!CpuFeatures::SupportsWasmSimd128()) {
    builder->SimdScalarLoweringForTesting();
  }
}

void TestBuildingGraph(Zone* zone, compiler::JSGraph* jsgraph,
                       CompilationEnv* module, FunctionSig* sig,
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
            return_type.representation(), WriteBarrierKind::kNoWriteBarrier)),
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

    auto call_descriptor =
        compiler::Linkage::GetSimplifiedCDescriptor(zone(), signature_, true);

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
                                  Code::C_WASM_ENTRY);
    code_ = compiler::Pipeline::GenerateCodeForTesting(
        &info, isolate, call_descriptor, graph(),
        AssemblerOptions::Default(isolate));
    code = code_.ToHandleChecked();
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code) {
      CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
      OFStream os(tracing_scope.file());

      code->Disassemble("wasm wrapper", os);
    }
#endif
  }

  return code;
}

void WasmFunctionCompiler::Build(const byte* start, const byte* end) {
  size_t locals_size = local_decls.Size();
  size_t total_size = end - start + locals_size + 1;
  byte* buffer = static_cast<byte*>(zone()->New(total_size));
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
                                         ->native_module()
                                         ->wire_bytes();

  CompilationEnv env = builder_->CreateCompilationEnv();
  ScopedVector<uint8_t> func_wire_bytes(function_->code.length());
  memcpy(func_wire_bytes.start(), wire_bytes.start() + function_->code.offset(),
         func_wire_bytes.length());

  FunctionBody func_body{function_->sig, function_->code.offset(),
                         func_wire_bytes.start(), func_wire_bytes.end()};
  NativeModule* native_module =
      builder_->instance_object()->module_object()->native_module();
  WasmCompilationUnit unit(function_->func_index, builder_->execution_tier());
  WasmFeatures unused_detected_features;
  WasmCompilationResult result = unit.ExecuteCompilation(
      isolate()->wasm_engine(), &env,
      native_module->compilation_state()->GetWireBytesStorage(),
      isolate()->counters(), &unused_detected_features);
  WasmCode* code = native_module->AddCompiledCode(std::move(result));
  DCHECK_NOT_NULL(code);
  if (WasmCode::ShouldBeLogged(isolate())) code->LogCode(isolate());
}

WasmFunctionCompiler::WasmFunctionCompiler(Zone* zone, FunctionSig* sig,
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

FunctionSig* WasmRunnerBase::CreateSig(MachineType return_type,
                                       Vector<MachineType> param_types) {
  int return_count = return_type.IsNone() ? 0 : 1;
  int param_count = param_types.length();

  // Allocate storage array in zone.
  ValueType* sig_types = zone_.NewArray<ValueType>(return_count + param_count);

  // Convert machine types to local types, and check that there are no
  // MachineType::None()'s in the parameters.
  int idx = 0;
  if (return_count) sig_types[idx++] = ValueTypes::ValueTypeFor(return_type);
  for (MachineType param : param_types) {
    CHECK_NE(MachineType::None(), param);
    sig_types[idx++] = ValueTypes::ValueTypeFor(param);
  }
  return new (&zone_) FunctionSig(return_count, param_count, sig_types);
}

// static
bool WasmRunnerBase::trap_happened;

}  // namespace wasm
}  // namespace internal
}  // namespace v8
