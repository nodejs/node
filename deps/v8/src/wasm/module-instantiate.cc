// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-instantiate.h"

#include "src/api/api-inl.h"
#include "src/asmjs/asm-js.h"
#include "src/base/atomicops.h"
#include "src/codegen/compiler.h"
#include "src/compiler/wasm-compiler.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/metrics.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/constant-expression-interface.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/pgo.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-subtyping.h"

#define TRACE(...)                                          \
  do {                                                      \
    if (v8_flags.trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8::internal::wasm {

namespace {

uint8_t* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<uint8_t*>(buffer.ToHandleChecked()->backing_store()) +
         offset;
}

using ImportWrapperQueue =
    WrapperQueue<WasmImportWrapperCache::CacheKey, const FunctionSig*,
                 WasmImportWrapperCache::CacheKeyHash>;

class CompileImportWrapperJob final : public JobTask {
 public:
  CompileImportWrapperJob(
      Counters* counters, NativeModule* native_module,
      ImportWrapperQueue* queue,
      WasmImportWrapperCache::ModificationScope* cache_scope)
      : counters_(counters),
        native_module_(native_module),
        queue_(queue),
        cache_scope_(cache_scope) {}

  size_t GetMaxConcurrency(size_t worker_count) const override {
    size_t flag_limit = static_cast<size_t>(
        std::max(1, v8_flags.wasm_num_compilation_tasks.value()));
    // Add {worker_count} to the queue size because workers might still be
    // processing units that have already been popped from the queue.
    return std::min(flag_limit, worker_count + queue_->size());
  }

  void Run(JobDelegate* delegate) override {
    TRACE_EVENT0("v8.wasm", "wasm.CompileImportWrapperJob.Run");
    while (base::Optional<std::pair<const WasmImportWrapperCache::CacheKey,
                                    const FunctionSig*>>
               key = queue_->pop()) {
      // TODO(wasm): Batch code publishing, to avoid repeated locking and
      // permission switching.
      CompileImportWrapper(native_module_, counters_, key->first.kind,
                           key->second, key->first.canonical_type_index,
                           key->first.expected_arity, key->first.suspend,
                           cache_scope_);
      if (delegate->ShouldYield()) return;
    }
  }

 private:
  Counters* const counters_;
  NativeModule* const native_module_;
  ImportWrapperQueue* const queue_;
  WasmImportWrapperCache::ModificationScope* const cache_scope_;
};

Handle<DescriptorArray> CreateArrayDescriptorArray(
    Isolate* isolate, const wasm::ArrayType* type) {
  uint32_t kDescriptorsCount = 1;
  Handle<DescriptorArray> descriptors =
      isolate->factory()->NewDescriptorArray(kDescriptorsCount);

  // TODO(ishell): cache Wasm field type in FieldType value.
  MaybeObject any_type = MaybeObject::FromObject(FieldType::Any());
  DCHECK(IsSmi(any_type));

  // Add descriptor for length property.
  PropertyDetails details(PropertyKind::kData, FROZEN, PropertyLocation::kField,
                          PropertyConstness::kConst,
                          Representation::WasmValue(), static_cast<int>(0));
  descriptors->Set(InternalIndex(0), *isolate->factory()->length_string(),
                   any_type, details);

  descriptors->Sort();
  return descriptors;
}

Handle<Map> CreateStructMap(Isolate* isolate, const WasmModule* module,
                            int struct_index, Handle<Map> opt_rtt_parent,
                            Handle<WasmInstanceObject> instance) {
  const wasm::StructType* type = module->struct_type(struct_index);
  const int inobject_properties = 0;
  // We have to use the variable size sentinel because the instance size
  // stored directly in a Map is capped at 255 pointer sizes.
  const int map_instance_size = kVariableSizeSentinel;
  const int real_instance_size = WasmStruct::Size(type);
  const InstanceType instance_type = WASM_STRUCT_TYPE;
  // TODO(jkummerow): If NO_ELEMENTS were supported, we could use that here.
  const ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
  Handle<WasmTypeInfo> type_info = isolate->factory()->NewWasmTypeInfo(
      reinterpret_cast<Address>(type), opt_rtt_parent, real_instance_size,
      instance, struct_index);
  Handle<Map> map = isolate->factory()->NewMap(
      instance_type, map_instance_size, elements_kind, inobject_properties);
  map->set_wasm_type_info(*type_info);
  map->SetInstanceDescriptors(isolate,
                              *isolate->factory()->empty_descriptor_array(), 0);
  map->set_is_extensible(false);
  WasmStruct::EncodeInstanceSizeInMap(real_instance_size, *map);
  return map;
}

Handle<Map> CreateArrayMap(Isolate* isolate, const WasmModule* module,
                           int array_index, Handle<Map> opt_rtt_parent,
                           Handle<WasmInstanceObject> instance) {
  const wasm::ArrayType* type = module->array_type(array_index);
  const int inobject_properties = 0;
  const int instance_size = kVariableSizeSentinel;
  // Wasm Arrays don't have a static instance size.
  const int cached_instance_size = 0;
  const InstanceType instance_type = WASM_ARRAY_TYPE;
  const ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
  Handle<WasmTypeInfo> type_info = isolate->factory()->NewWasmTypeInfo(
      reinterpret_cast<Address>(type), opt_rtt_parent, cached_instance_size,
      instance, array_index);
  // TODO(ishell): get canonical descriptor array for WasmArrays from roots.
  Handle<DescriptorArray> descriptors =
      CreateArrayDescriptorArray(isolate, type);
  Handle<Map> map = isolate->factory()->NewMap(
      instance_type, instance_size, elements_kind, inobject_properties);
  map->set_wasm_type_info(*type_info);
  map->SetInstanceDescriptors(isolate, *descriptors,
                              descriptors->number_of_descriptors());
  map->set_is_extensible(false);
  WasmArray::EncodeElementSizeInMap(type->element_type().value_kind_size(),
                                    *map);
  return map;
}

void CreateMapForType(Isolate* isolate, const WasmModule* module,
                      int type_index, Handle<WasmInstanceObject> instance,
                      Handle<FixedArray> maps) {
  // Recursive calls for supertypes may already have created this map.
  if (IsMap(maps->get(type_index))) return;

  Handle<WeakArrayList> canonical_rtts;
  uint32_t canonical_type_index =
      module->isorecursive_canonical_type_ids[type_index];

  // Try to find the canonical map for this type in the isolate store.
  canonical_rtts = handle(isolate->heap()->wasm_canonical_rtts(), isolate);
  DCHECK_GT(static_cast<uint32_t>(canonical_rtts->length()),
            canonical_type_index);
  MaybeObject maybe_canonical_map = canonical_rtts->Get(canonical_type_index);
  if (maybe_canonical_map.IsStrongOrWeak() &&
      IsMap(maybe_canonical_map.GetHeapObject())) {
    maps->set(type_index, maybe_canonical_map.GetHeapObject());
    return;
  }

  Handle<Map> rtt_parent;
  // If the type with {type_index} has an explicit supertype, make sure the
  // map for that supertype is created first, so that the supertypes list
  // that's cached on every RTT can be set up correctly.
  uint32_t supertype = module->supertype(type_index);
  if (supertype != kNoSuperType) {
    // This recursion is safe, because kV8MaxRttSubtypingDepth limits the
    // number of recursive steps, so we won't overflow the stack.
    CreateMapForType(isolate, module, supertype, instance, maps);
    rtt_parent = handle(Map::cast(maps->get(supertype)), isolate);
  }
  Handle<Map> map;
  switch (module->types[type_index].kind) {
    case TypeDefinition::kStruct:
      map = CreateStructMap(isolate, module, type_index, rtt_parent, instance);
      break;
    case TypeDefinition::kArray:
      map = CreateArrayMap(isolate, module, type_index, rtt_parent, instance);
      break;
    case TypeDefinition::kFunction:
      map = CreateFuncRefMap(isolate, rtt_parent, instance);
      break;
  }
  canonical_rtts->Set(canonical_type_index, HeapObjectReference::Weak(*map));
  maps->set(type_index, *map);
}

MachineRepresentation NormalizeFastApiRepresentation(const CTypeInfo& info) {
  MachineType t = MachineType::TypeForCType(info);
  // Wasm representation of bool is i32 instead of i1.
  if (t.semantic() == MachineSemantic::kBool) {
    return MachineRepresentation::kWord32;
  }
  return t.representation();
}

bool IsSupportedWasmFastApiFunction(Isolate* isolate,
                                    const wasm::FunctionSig* expected_sig,
                                    Handle<SharedFunctionInfo> shared) {
  if (!shared->IsApiFunction()) {
    return false;
  }
  if (shared->api_func_data()->GetCFunctionsCount() == 0) {
    return false;
  }
  if (!shared->api_func_data()->accept_any_receiver()) {
    return false;
  }
  if (!IsUndefined(shared->api_func_data()->signature())) {
    // TODO(wasm): CFunctionInfo* signature check.
    return false;
  }
  const CFunctionInfo* info = shared->api_func_data()->GetCSignature(0);
  if (!compiler::IsFastCallSupportedSignature(info)) {
    return false;
  }

  const auto log_imported_function_mismatch = [&shared,
                                               isolate](const char* reason) {
    if (v8_flags.trace_opt) {
      CodeTracer::Scope scope(isolate->GetCodeTracer());
      PrintF(scope.file(), "[disabled optimization for ");
      ShortPrint(*shared, scope.file());
      PrintF(scope.file(),
             ", reason: the signature of the imported function in the Wasm "
             "module doesn't match that of the Fast API function (%s)]\n",
             reason);
    }
  };

  // C functions only have one return value.
  if (expected_sig->return_count() > 1) {
    // Here and below, we log when the function we call is declared as an Api
    // function but we cannot optimize the call, which might be unxepected. In
    // that case we use the "slow" path making a normal Wasm->JS call and
    // calling the "slow" callback specified in FunctionTemplate::New().
    log_imported_function_mismatch("too many return values");
    return false;
  }
  CTypeInfo return_info = info->ReturnInfo();
  // Unsupported if return type doesn't match.
  if (expected_sig->return_count() == 0 &&
      return_info.GetType() != CTypeInfo::Type::kVoid) {
    log_imported_function_mismatch("too few return values");
    return false;
  }
  // Unsupported if return type doesn't match.
  if (expected_sig->return_count() == 1) {
    if (return_info.GetType() == CTypeInfo::Type::kVoid) {
      log_imported_function_mismatch("too many return values");
      return false;
    }
    if (NormalizeFastApiRepresentation(return_info) !=
        expected_sig->GetReturn(0).machine_type().representation()) {
      log_imported_function_mismatch("mismatching return value");
      return false;
    }
  }
  // Unsupported if arity doesn't match.
  if (expected_sig->parameter_count() != info->ArgumentCount() - 1) {
    log_imported_function_mismatch("mismatched arity");
    return false;
  }
  // Unsupported if any argument types don't match.
  for (unsigned int i = 0; i < expected_sig->parameter_count(); i += 1) {
    // Arg 0 is the receiver, skip over it since wasm doesn't
    // have a concept of receivers.
    CTypeInfo arg = info->ArgumentInfo(i + 1);
    if (NormalizeFastApiRepresentation(arg) !=
        expected_sig->GetParam(i).machine_type().representation()) {
      log_imported_function_mismatch("parameter type mismatch");
      return false;
    }
  }
  return true;
}

bool ResolveBoundJSFastApiFunction(const wasm::FunctionSig* expected_sig,
                                   Handle<JSReceiver> callable) {
  Handle<JSFunction> target;
  if (IsJSBoundFunction(*callable)) {
    Handle<JSBoundFunction> bound_target =
        Handle<JSBoundFunction>::cast(callable);
    // Nested bound functions and arguments not supported yet.
    if (bound_target->bound_arguments()->length() > 0) {
      return false;
    }
    if (IsJSBoundFunction(bound_target->bound_target_function())) {
      return false;
    }
    Handle<JSReceiver> bound_target_function =
        handle(bound_target->bound_target_function(), callable->GetIsolate());
    if (!IsJSFunction(*bound_target_function)) {
      return false;
    }
    target = Handle<JSFunction>::cast(bound_target_function);
  } else if (IsJSFunction(*callable)) {
    target = Handle<JSFunction>::cast(callable);
  } else {
    return false;
  }

  Isolate* isolate = target->GetIsolate();
  Handle<SharedFunctionInfo> shared(target->shared(), isolate);
  return IsSupportedWasmFastApiFunction(isolate, expected_sig, shared);
}

bool IsStringRef(wasm::ValueType type) {
  return type.is_reference_to(wasm::HeapType::kString);
}

bool IsExternRef(wasm::ValueType type) {
  return type.is_reference_to(wasm::HeapType::kExtern);
}

bool IsStringOrExternRef(wasm::ValueType type) {
  return IsStringRef(type) || IsExternRef(type);
}

bool IsI16Array(wasm::ValueType type, const WasmModule* module, bool writable) {
  if (!type.is_object_reference() || !type.has_index()) return false;
  uint32_t reftype = type.ref_index();
  if (!module->has_array(reftype)) return false;
  if (writable && !module->array_type(reftype)->mutability()) return false;
  return module->array_type(reftype)->element_type() == kWasmI16;
}

bool IsI8Array(wasm::ValueType type, const WasmModule* module, bool writable) {
  if (!type.is_object_reference() || !type.has_index()) return false;
  uint32_t reftype = type.ref_index();
  if (!module->has_array(reftype)) return false;
  if (writable && !module->array_type(reftype)->mutability()) return false;
  return module->array_type(reftype)->element_type() == kWasmI8;
}

// This detects imports of the forms:
// - `Function.prototype.call.bind(foo)`, where `foo` is something that has a
//   Builtin id.
// - JSFunction with Builtin id (e.g. `parseFloat`).
WellKnownImport CheckForWellKnownImport(Handle<WasmInstanceObject> instance,
                                        int func_index,
                                        Handle<JSReceiver> callable,
                                        const wasm::FunctionSig* sig) {
  WellKnownImport kGeneric = WellKnownImport::kGeneric;  // "using" is C++20.
  if (instance.is_null()) return kGeneric;
  static constexpr ValueType kRefExtern = ValueType::Ref(HeapType::kExtern);
  // Check for plain JS functions.
  if (IsJSFunction(*callable)) {
    Tagged<SharedFunctionInfo> sfi = JSFunction::cast(*callable)->shared();
    if (!sfi->HasBuiltinId()) return kGeneric;
    // This needs to be a separate switch because it allows other cases than
    // the one below. Merging them would be invalid, because we would then
    // recognize receiver-requiring methods even when they're (erroneously)
    // being imported such that they don't get a receiver.
    switch (sfi->builtin_id()) {
      case Builtin::kWebAssemblyStringCharCodeAt:
        if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetParam(1) == kWasmI32 && sig->GetReturn(0) == kWasmI32) {
          return WellKnownImport::kStringCharCodeAt;
        }
        break;
      case Builtin::kWebAssemblyStringCodePointAt:
        if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetParam(1) == kWasmI32 && sig->GetReturn(0) == kWasmI32) {
          return WellKnownImport::kStringCodePointAt;
        }
        break;
      case Builtin::kWebAssemblyStringCompare:
        if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetParam(1) == kWasmExternRef &&
            sig->GetReturn(0) == kWasmI32) {
          return WellKnownImport::kStringCompare;
        }
        break;
      case Builtin::kWebAssemblyStringConcat:
        if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetParam(1) == kWasmExternRef &&
            sig->GetReturn(0) == kRefExtern) {
          return WellKnownImport::kStringConcat;
        }
        break;
      case Builtin::kWebAssemblyStringEquals:
        if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetParam(1) == kWasmExternRef &&
            sig->GetReturn(0) == kWasmI32) {
          return WellKnownImport::kStringEquals;
        }
        break;
      case Builtin::kWebAssemblyStringFromCharCode:
        if (sig->parameter_count() == 1 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmI32 && sig->GetReturn(0) == kRefExtern) {
          return WellKnownImport::kStringFromCharCode;
        }
        break;
      case Builtin::kWebAssemblyStringFromCodePoint:
        if (sig->parameter_count() == 1 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmI32 && sig->GetReturn(0) == kRefExtern) {
          return WellKnownImport::kStringFromCodePoint;
        }
        break;
      case Builtin::kWebAssemblyStringFromWtf16Array:
        // i16array, i32, i32 -> extern
        if (sig->parameter_count() == 3 && sig->return_count() == 1 &&
            IsI16Array(sig->GetParam(0), instance->module(), false) &&
            sig->GetParam(1) == kWasmI32 && sig->GetParam(2) == kWasmI32 &&
            sig->GetReturn(0) == kRefExtern) {
          return WellKnownImport::kStringFromWtf16Array;
        }
        break;
      case Builtin::kWebAssemblyStringFromWtf8Array:
        // i8array, i32, i32 -> extern
        if (sig->parameter_count() == 3 && sig->return_count() == 1 &&
            IsI8Array(sig->GetParam(0), instance->module(), false) &&
            sig->GetParam(1) == kWasmI32 && sig->GetParam(2) == kWasmI32 &&
            sig->GetReturn(0) == kRefExtern) {
          return WellKnownImport::kStringFromWtf8Array;
        }
        break;
      case Builtin::kWebAssemblyStringLength:
        if (sig->parameter_count() == 1 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetReturn(0) == kWasmI32) {
          return WellKnownImport::kStringLength;
        }
        break;
      case Builtin::kWebAssemblyStringSubstring:
        if (sig->parameter_count() == 3 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            sig->GetParam(1) == kWasmI32 && sig->GetParam(2) == kWasmI32 &&
            sig->GetReturn(0) == kRefExtern) {
          return WellKnownImport::kStringSubstring;
        }
        break;
      case Builtin::kWebAssemblyStringToWtf16Array:
        // string, i16array, i32 -> i32
        if (sig->parameter_count() == 3 && sig->return_count() == 1 &&
            sig->GetParam(0) == kWasmExternRef &&
            IsI16Array(sig->GetParam(1), instance->module(), true) &&
            sig->GetParam(2) == kWasmI32 && sig->GetReturn(0) == kWasmI32) {
          return WellKnownImport::kStringToWtf16Array;
        }
        break;
      case Builtin::kNumberParseFloat:
        if (sig->parameter_count() == 1 && sig->return_count() == 1 &&
            IsStringRef(sig->GetParam(0)) &&
            sig->GetReturn(0) == wasm::kWasmF64) {
          return WellKnownImport::kParseFloat;
        }
        break;
      default:
        break;
    }
    return kGeneric;
  }

  // Check for bound JS functions.
  // First part: check that the callable is a bound function whose target
  // is {Function.prototype.call}, and which only binds a receiver.
  if (!IsJSBoundFunction(*callable)) return kGeneric;
  Handle<JSBoundFunction> bound = Handle<JSBoundFunction>::cast(callable);
  if (bound->bound_arguments()->length() != 0) return kGeneric;
  if (!IsJSFunction(bound->bound_target_function())) return kGeneric;
  Tagged<SharedFunctionInfo> sfi =
      JSFunction::cast(bound->bound_target_function())->shared();
  if (!sfi->HasBuiltinId()) return kGeneric;
  if (sfi->builtin_id() != Builtin::kFunctionPrototypeCall) return kGeneric;
  // Second part: check if the bound receiver is one of the builtins for which
  // we have special-cased support.
  Tagged<Object> bound_this = bound->bound_this();
  if (!IsJSFunction(bound_this)) return kGeneric;
  sfi = JSFunction::cast(bound_this)->shared();
  if (!sfi->HasBuiltinId()) return kGeneric;
  switch (sfi->builtin_id()) {
#if V8_INTL_SUPPORT
    case Builtin::kStringPrototypeToLocaleLowerCase:
      if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
          IsStringRef(sig->GetParam(0)) && IsStringRef(sig->GetParam(1)) &&
          IsStringRef(sig->GetReturn(0))) {
        DCHECK_GE(func_index, 0);
        instance->well_known_imports()->set(func_index, bound_this);
        return WellKnownImport::kStringToLocaleLowerCaseStringref;
      }
      break;
    case Builtin::kStringPrototypeToLowerCaseIntl:
      if (sig->parameter_count() == 1 && sig->return_count() == 1 &&
          IsStringRef(sig->GetParam(0)) && IsStringRef(sig->GetReturn(0))) {
        return WellKnownImport::kStringToLowerCaseStringref;
      }
      break;
#endif
    case Builtin::kNumberPrototypeToString:
      if (sig->parameter_count() == 2 && sig->return_count() == 1 &&
          sig->GetParam(0) == wasm::kWasmI32 &&
          sig->GetParam(1) == wasm::kWasmI32 &&
          IsStringOrExternRef(sig->GetReturn(0))) {
        return WellKnownImport::kIntToString;
      }
      if (sig->parameter_count() == 1 && sig->return_count() == 1 &&
          sig->GetParam(0) == wasm::kWasmF64 &&
          IsStringOrExternRef(sig->GetReturn(0))) {
        return WellKnownImport::kDoubleToString;
      }
      break;
    case Builtin::kStringPrototypeIndexOf:
      // (string, string, i32) -> (i32).
      if (sig->parameter_count() == 3 && sig->return_count() == 1 &&
          IsStringRef(sig->GetParam(0)) && IsStringRef(sig->GetParam(1)) &&
          sig->GetParam(2) == wasm::kWasmI32 &&
          sig->GetReturn(0) == wasm::kWasmI32)
        return WellKnownImport::kStringIndexOf;
      break;
    default:
      break;
  }
  return kGeneric;
}

}  // namespace

WasmImportData::WasmImportData(Handle<WasmInstanceObject> instance,
                               int func_index, Handle<JSReceiver> callable,
                               const wasm::FunctionSig* expected_sig,
                               uint32_t expected_canonical_type_index)
    : callable_(callable) {
  kind_ = ComputeKind(instance, func_index, expected_sig,
                      expected_canonical_type_index);
}

ImportCallKind WasmImportData::ComputeKind(
    Handle<WasmInstanceObject> instance, int func_index,
    const wasm::FunctionSig* expected_sig,
    uint32_t expected_canonical_type_index) {
  Isolate* isolate = callable_->GetIsolate();
  if (WasmExportedFunction::IsWasmExportedFunction(*callable_)) {
    auto imported_function = Handle<WasmExportedFunction>::cast(callable_);
    if (!imported_function->MatchesSignature(expected_canonical_type_index)) {
      return ImportCallKind::kLinkError;
    }
    uint32_t func_index =
        static_cast<uint32_t>(imported_function->function_index());
    if (func_index >=
        imported_function->instance()->module()->num_imported_functions) {
      return ImportCallKind::kWasmToWasm;
    }
    // Resolve the shortcut to the underlying callable and continue.
    Handle<WasmInstanceObject> instance(imported_function->instance(), isolate);
    ImportedFunctionEntry entry(instance, func_index);
    callable_ = handle(entry.callable(), isolate);
  }
  if (WasmJSFunction::IsWasmJSFunction(*callable_)) {
    auto js_function = Handle<WasmJSFunction>::cast(callable_);
    suspend_ = js_function->GetSuspend();
    if (!js_function->MatchesSignature(expected_canonical_type_index)) {
      return ImportCallKind::kLinkError;
    }
    // Resolve the short-cut to the underlying callable and continue.
    callable_ = handle(js_function->GetCallable(), isolate);
  }
  if (WasmCapiFunction::IsWasmCapiFunction(*callable_)) {
    auto capi_function = Handle<WasmCapiFunction>::cast(callable_);
    if (!capi_function->MatchesSignature(expected_canonical_type_index)) {
      return ImportCallKind::kLinkError;
    }
    return ImportCallKind::kWasmToCapi;
  }
  // Assuming we are calling to JS, check whether this would be a runtime error.
  if (!wasm::IsJSCompatibleSignature(expected_sig)) {
    return ImportCallKind::kRuntimeTypeError;
  }
  // Check if this can be a JS fast API call.
  if (v8_flags.turbo_fast_api_calls &&
      ResolveBoundJSFastApiFunction(expected_sig, callable_)) {
    return ImportCallKind::kWasmToJSFastApi;
  }
  well_known_status_ =
      CheckForWellKnownImport(instance, func_index, callable_, expected_sig);
  // For JavaScript calls, determine whether the target has an arity match.
  if (IsJSFunction(*callable_)) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callable_);
    Handle<SharedFunctionInfo> shared(function->shared(),
                                      function->GetIsolate());

// Check for math intrinsics.
#define COMPARE_SIG_FOR_BUILTIN(name)                                     \
  {                                                                       \
    const wasm::FunctionSig* sig =                                        \
        wasm::WasmOpcodes::Signature(wasm::kExpr##name);                  \
    if (!sig) sig = wasm::WasmOpcodes::AsmjsSignature(wasm::kExpr##name); \
    DCHECK_NOT_NULL(sig);                                                 \
    if (*expected_sig == *sig) {                                          \
      return ImportCallKind::k##name;                                     \
    }                                                                     \
  }
#define COMPARE_SIG_FOR_BUILTIN_F64(name) \
  case Builtin::kMath##name:              \
    COMPARE_SIG_FOR_BUILTIN(F64##name);   \
    break;
#define COMPARE_SIG_FOR_BUILTIN_F32_F64(name) \
  case Builtin::kMath##name:                  \
    COMPARE_SIG_FOR_BUILTIN(F64##name);       \
    COMPARE_SIG_FOR_BUILTIN(F32##name);       \
    break;

    if (v8_flags.wasm_math_intrinsics && shared->HasBuiltinId()) {
      switch (shared->builtin_id()) {
        COMPARE_SIG_FOR_BUILTIN_F64(Acos);
        COMPARE_SIG_FOR_BUILTIN_F64(Asin);
        COMPARE_SIG_FOR_BUILTIN_F64(Atan);
        COMPARE_SIG_FOR_BUILTIN_F64(Cos);
        COMPARE_SIG_FOR_BUILTIN_F64(Sin);
        COMPARE_SIG_FOR_BUILTIN_F64(Tan);
        COMPARE_SIG_FOR_BUILTIN_F64(Exp);
        COMPARE_SIG_FOR_BUILTIN_F64(Log);
        COMPARE_SIG_FOR_BUILTIN_F64(Atan2);
        COMPARE_SIG_FOR_BUILTIN_F64(Pow);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Min);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Max);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Abs);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Ceil);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Floor);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Sqrt);
        case Builtin::kMathFround:
          COMPARE_SIG_FOR_BUILTIN(F32ConvertF64);
          break;
        default:
          break;
      }
    }

#undef COMPARE_SIG_FOR_BUILTIN
#undef COMPARE_SIG_FOR_BUILTIN_F64
#undef COMPARE_SIG_FOR_BUILTIN_F32_F64

    if (IsClassConstructor(shared->kind())) {
      // Class constructor will throw anyway.
      return ImportCallKind::kUseCallBuiltin;
    }

    if (shared->internal_formal_parameter_count_without_receiver() ==
        expected_sig->parameter_count() - suspend_) {
      return ImportCallKind::kJSFunctionArityMatch;
    }

    // If function isn't compiled, compile it now.
    Isolate* isolate = callable_->GetIsolate();
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));
    if (!is_compiled_scope.is_compiled()) {
      Compiler::Compile(isolate, function, Compiler::CLEAR_EXCEPTION,
                        &is_compiled_scope);
    }

    return ImportCallKind::kJSFunctionArityMismatch;
  }
  // Unknown case. Use the call builtin.
  return ImportCallKind::kUseCallBuiltin;
}

// A helper class to simplify instantiating a module from a module object.
// It closes over the {Isolate}, the {ErrorThrower}, etc.
class InstanceBuilder {
 public:
  InstanceBuilder(Isolate* isolate, v8::metrics::Recorder::ContextId context_id,
                  ErrorThrower* thrower, Handle<WasmModuleObject> module_object,
                  MaybeHandle<JSReceiver> ffi,
                  MaybeHandle<JSArrayBuffer> memory_buffer);

  // Build an instance, in all of its glory.
  MaybeHandle<WasmInstanceObject> Build();
  // Run the start function, if any.
  bool ExecuteStartFunction();

 private:
  // A pre-evaluated value to use in import binding.
  struct SanitizedImport {
    Handle<String> module_name;
    Handle<String> import_name;
    Handle<Object> value;
  };

  Isolate* isolate_;
  v8::metrics::Recorder::ContextId context_id_;
  const WasmFeatures enabled_;
  const WasmModule* const module_;
  ErrorThrower* thrower_;
  Handle<WasmModuleObject> module_object_;
  MaybeHandle<JSReceiver> ffi_;
  MaybeHandle<JSArrayBuffer> asmjs_memory_buffer_;
  Handle<JSArrayBuffer> untagged_globals_;
  Handle<FixedArray> tagged_globals_;
  std::vector<Handle<WasmTagObject>> tags_wrappers_;
  Handle<WasmExportedFunction> start_function_;
  std::vector<SanitizedImport> sanitized_imports_;
  std::vector<WellKnownImport> well_known_imports_;
  // We pass this {Zone} to the temporary {WasmFullDecoder} we allocate during
  // each call to {EvaluateConstantExpression}. This has been found to improve
  // performance a bit over allocating a new {Zone} each time.
  Zone init_expr_zone_;

  std::string ImportName(uint32_t index, Handle<String> module_name,
                         Handle<String> import_name) {
    std::ostringstream oss;
    oss << "Import #" << index << " module=\"" << module_name->ToCString().get()
        << "\" function=\"" << import_name->ToCString().get() << "\"";
    return oss.str();
  }

  std::string ImportName(uint32_t index, Handle<String> module_name) {
    std::ostringstream oss;
    oss << "Import #" << index << " module=\"" << module_name->ToCString().get()
        << "\"";
    return oss.str();
  }

  // Look up an import value in the {ffi_} object.
  MaybeHandle<Object> LookupImport(uint32_t index, Handle<String> module_name,
                                   Handle<String> import_name);

  // Look up an import value in the {ffi_} object specifically for linking an
  // asm.js module. This only performs non-observable lookups, which allows
  // falling back to JavaScript proper (and hence re-executing all lookups) if
  // module instantiation fails.
  MaybeHandle<Object> LookupImportAsm(uint32_t index,
                                      Handle<String> import_name);

  // Load data segments into the memory.
  void LoadDataSegments(Handle<WasmInstanceObject> instance);

  void WriteGlobalValue(const WasmGlobal& global, const WasmValue& value);

  void SanitizeImports();

  // Allocate the memory.
  MaybeHandle<WasmMemoryObject> AllocateMemory(uint32_t memory_index);

  // Processes a single imported function.
  bool ProcessImportedFunction(Handle<WasmInstanceObject> instance,
                               int import_index, int func_index,
                               Handle<String> module_name,
                               Handle<String> import_name,
                               Handle<Object> value);

  // Initialize imported tables of type funcref.
  bool InitializeImportedIndirectFunctionTable(
      Handle<WasmInstanceObject> instance, int table_index, int import_index,
      Handle<WasmTableObject> table_object);

  // Process a single imported table.
  bool ProcessImportedTable(Handle<WasmInstanceObject> instance,
                            int import_index, int table_index,
                            Handle<String> module_name,
                            Handle<String> import_name, Handle<Object> value);

  // Process a single imported global.
  bool ProcessImportedGlobal(Handle<WasmInstanceObject> instance,
                             int import_index, int global_index,
                             Handle<String> module_name,
                             Handle<String> import_name, Handle<Object> value);

  // Process a single imported WasmGlobalObject.
  bool ProcessImportedWasmGlobalObject(Handle<WasmInstanceObject> instance,
                                       int import_index,
                                       Handle<String> module_name,
                                       Handle<String> import_name,
                                       const WasmGlobal& global,
                                       Handle<WasmGlobalObject> global_object);

  // Compile import wrappers in parallel. The result goes into the native
  // module's import_wrapper_cache.
  void CompileImportWrappers(Handle<WasmInstanceObject> instance);

  // Process the imports, including functions, tables, globals, and memory, in
  // order, loading them from the {ffi_} object. Returns the number of imported
  // functions, or {-1} on error.
  int ProcessImports(Handle<WasmInstanceObject> instance);

  // Process all imported memories, placing the WasmMemoryObjects in the
  // supplied {FixedArray}.
  bool ProcessImportedMemories(Handle<FixedArray> imported_memory_objects);

  template <typename T>
  T* GetRawUntaggedGlobalPtr(const WasmGlobal& global);

  // Process initialization of globals.
  void InitGlobals(Handle<WasmInstanceObject> instance);

  // Process the exports, creating wrappers for functions, tables, memories,
  // and globals.
  void ProcessExports(Handle<WasmInstanceObject> instance);

  void SetTableInitialValues(Handle<WasmInstanceObject> instance);

  void LoadTableSegments(Handle<WasmInstanceObject> instance);

  // Creates new tags. Note that some tags might already exist if they were
  // imported, those tags will be re-used.
  void InitializeTags(Handle<WasmInstanceObject> instance);
};

namespace {
class ReportLazyCompilationTimesTask : public v8::Task {
 public:
  ReportLazyCompilationTimesTask(std::weak_ptr<Counters> counters,
                                 std::weak_ptr<NativeModule> native_module,
                                 int delay_in_seconds)
      : counters_(std::move(counters)),
        native_module_(std::move(native_module)),
        delay_in_seconds_(delay_in_seconds) {}

  void Run() final {
    std::shared_ptr<NativeModule> native_module = native_module_.lock();
    if (!native_module) return;
    std::shared_ptr<Counters> counters = counters_.lock();
    if (!counters) return;
    int num_compilations = native_module->num_lazy_compilations();
    // If no compilations happened, we don't add samples. Experiments showed
    // many cases of num_compilations == 0, and adding these cases would make
    // other cases less visible.
    if (!num_compilations) return;
    if (delay_in_seconds_ == 5) {
      counters->wasm_num_lazy_compilations_5sec()->AddSample(num_compilations);
      counters->wasm_sum_lazy_compilation_time_5sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_5sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    if (delay_in_seconds_ == 20) {
      counters->wasm_num_lazy_compilations_20sec()->AddSample(num_compilations);
      counters->wasm_sum_lazy_compilation_time_20sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_20sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    if (delay_in_seconds_ == 60) {
      counters->wasm_num_lazy_compilations_60sec()->AddSample(num_compilations);
      counters->wasm_sum_lazy_compilation_time_60sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_60sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    if (delay_in_seconds_ == 120) {
      counters->wasm_num_lazy_compilations_120sec()->AddSample(
          num_compilations);
      counters->wasm_sum_lazy_compilation_time_120sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_120sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    UNREACHABLE();
  }

 private:
  std::weak_ptr<Counters> counters_;
  std::weak_ptr<NativeModule> native_module_;
  int delay_in_seconds_;
};

class WriteOutPGOTask : public v8::Task {
 public:
  explicit WriteOutPGOTask(std::weak_ptr<NativeModule> native_module)
      : native_module_(std::move(native_module)) {}

  void Run() final {
    std::shared_ptr<NativeModule> native_module = native_module_.lock();
    if (!native_module) return;
    DumpProfileToFile(native_module->module(), native_module->wire_bytes(),
                      native_module->tiering_budget_array());
    Schedule(std::move(native_module_));
  }

  static void Schedule(std::weak_ptr<NativeModule> native_module) {
    // Write out PGO info every 10 seconds.
    V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
        std::make_unique<WriteOutPGOTask>(std::move(native_module)), 10.0);
  }

 private:
  const std::weak_ptr<NativeModule> native_module_;
};

}  // namespace

MaybeHandle<WasmInstanceObject> InstantiateToInstanceObject(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory_buffer) {
  v8::metrics::Recorder::ContextId context_id =
      isolate->GetOrRegisterRecorderContextId(isolate->native_context());
  InstanceBuilder builder(isolate, context_id, thrower, module_object, imports,
                          memory_buffer);
  auto instance = builder.Build();
  if (!instance.is_null()) {
    const std::shared_ptr<NativeModule>& native_module =
        module_object->shared_native_module();
    // Post tasks for lazy compilation metrics before we call the start
    // function.
    if (v8_flags.wasm_lazy_compilation &&
        native_module->ShouldLazyCompilationMetricsBeReported()) {
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), native_module, 5),
          5.0);
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), native_module, 20),
          20.0);
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), native_module, 60),
          60.0);
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), native_module, 120),
          120.0);
    }
    if (v8_flags.experimental_wasm_pgo_to_file &&
        native_module->ShouldPgoDataBeWritten() &&
        native_module->module()->num_declared_functions > 0) {
      WriteOutPGOTask::Schedule(native_module);
    }
    if (builder.ExecuteStartFunction()) {
      return instance;
    }
  }
  DCHECK(isolate->has_pending_exception() || thrower->error());
  return {};
}

InstanceBuilder::InstanceBuilder(Isolate* isolate,
                                 v8::metrics::Recorder::ContextId context_id,
                                 ErrorThrower* thrower,
                                 Handle<WasmModuleObject> module_object,
                                 MaybeHandle<JSReceiver> ffi,
                                 MaybeHandle<JSArrayBuffer> asmjs_memory_buffer)
    : isolate_(isolate),
      context_id_(context_id),
      enabled_(module_object->native_module()->enabled_features()),
      module_(module_object->module()),
      thrower_(thrower),
      module_object_(module_object),
      ffi_(ffi),
      asmjs_memory_buffer_(asmjs_memory_buffer),
      init_expr_zone_(isolate_->allocator(), "constant expression zone") {
  sanitized_imports_.reserve(module_->import_table.size());
  well_known_imports_.reserve(module_->num_imported_functions);
}

// Build an instance, in all of its glory.
MaybeHandle<WasmInstanceObject> InstanceBuilder::Build() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.InstanceBuilder.Build");
  // Check that an imports argument was provided, if the module requires it.
  // No point in continuing otherwise.
  if (!module_->import_table.empty() && ffi_.is_null()) {
    thrower_->TypeError(
        "Imports argument must be present and must be an object");
    return {};
  }

  SanitizeImports();
  if (thrower_->error()) return {};

  // From here on, we expect the build pipeline to run without exiting to JS.
  DisallowJavascriptExecution no_js(isolate_);
  // Start a timer for instantiation time, if we have a high resolution timer.
  base::ElapsedTimer timer;
  if (base::TimeTicks::IsHighResolution()) {
    timer.Start();
  }
  v8::metrics::WasmModuleInstantiated wasm_module_instantiated;
  NativeModule* native_module = module_object_->native_module();

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Instance object.
  //--------------------------------------------------------------------------
  TRACE("New module instantiation for %p\n", native_module);
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, module_object_);

  //--------------------------------------------------------------------------
  // Set up the memory buffers and memory objects and attach them to the
  // instance.
  //--------------------------------------------------------------------------
  if (is_asmjs_module(module_)) {
    CHECK_EQ(1, module_->memories.size());
    Handle<JSArrayBuffer> buffer;
    if (!asmjs_memory_buffer_.ToHandle(&buffer)) {
      // Use an empty JSArrayBuffer for degenerate asm.js modules.
      MaybeHandle<JSArrayBuffer> new_buffer =
          isolate_->factory()->NewJSArrayBufferAndBackingStore(
              0, InitializedFlag::kUninitialized);
      if (!new_buffer.ToHandle(&buffer)) {
        thrower_->RangeError("Out of memory: asm.js memory");
        return {};
      }
      buffer->set_is_detachable(false);
    }
    // asm.js instantiation should have changed the state of the buffer (or we
    // set it above).
    CHECK(!buffer->is_detachable());

    // The maximum number of pages isn't strictly necessary for memory
    // objects used for asm.js, as they are never visible, but we might
    // as well make it accurate.
    auto maximum_pages =
        static_cast<int>(RoundUp(buffer->byte_length(), wasm::kWasmPageSize) /
                         wasm::kWasmPageSize);
    Handle<WasmMemoryObject> memory_object = WasmMemoryObject::New(
        isolate_, buffer, maximum_pages, WasmMemoryFlag::kWasmMemory32);
    constexpr int kMemoryIndexZero = 0;
    WasmMemoryObject::UseInInstance(isolate_, memory_object, instance,
                                    kMemoryIndexZero);
    instance->memory_objects()->set(kMemoryIndexZero, *memory_object);
  } else {
    CHECK(asmjs_memory_buffer_.is_null());
    Handle<FixedArray> memory_objects{instance->memory_objects(), isolate_};
    // First process all imported memories, then allocate non-imported ones.
    if (!ProcessImportedMemories(memory_objects)) return {};
    // Actual Wasm modules can have multiple memories.
    static_assert(kV8MaxWasmMemories <= kMaxUInt32);
    uint32_t num_memories = static_cast<uint32_t>(module_->memories.size());
    for (uint32_t memory_index = 0; memory_index < num_memories;
         ++memory_index) {
      Handle<WasmMemoryObject> memory_object;
      if (!IsUndefined(memory_objects->get(memory_index))) {
        memory_object =
            handle(WasmMemoryObject::cast(memory_objects->get(memory_index)),
                   isolate_);
      } else if (AllocateMemory(memory_index).ToHandle(&memory_object)) {
        memory_objects->set(memory_index, *memory_object);
      } else {
        DCHECK(isolate_->has_pending_exception() || thrower_->error());
        return {};
      }
      WasmMemoryObject::UseInInstance(isolate_, memory_object, instance,
                                      memory_index);
    }
  }

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
  uint32_t untagged_globals_buffer_size = module_->untagged_globals_buffer_size;
  if (untagged_globals_buffer_size > 0) {
    MaybeHandle<JSArrayBuffer> result =
        isolate_->factory()->NewJSArrayBufferAndBackingStore(
            untagged_globals_buffer_size, InitializedFlag::kZeroInitialized,
            AllocationType::kOld);

    if (!result.ToHandle(&untagged_globals_)) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }

    instance->set_untagged_globals_buffer(*untagged_globals_);
    instance->set_globals_start(
        reinterpret_cast<uint8_t*>(untagged_globals_->backing_store()));
  }

  uint32_t tagged_globals_buffer_size = module_->tagged_globals_buffer_size;
  if (tagged_globals_buffer_size > 0) {
    tagged_globals_ = isolate_->factory()->NewFixedArray(
        static_cast<int>(tagged_globals_buffer_size));
    instance->set_tagged_globals_buffer(*tagged_globals_);
  }

  //--------------------------------------------------------------------------
  // Set up the array of references to imported globals' array buffers.
  //--------------------------------------------------------------------------
  if (module_->num_imported_mutable_globals > 0) {
    // TODO(binji): This allocates one slot for each mutable global, which is
    // more than required if multiple globals are imported from the same
    // module.
    Handle<FixedArray> buffers_array = isolate_->factory()->NewFixedArray(
        module_->num_imported_mutable_globals, AllocationType::kOld);
    instance->set_imported_mutable_globals_buffers(*buffers_array);
  }

  //--------------------------------------------------------------------------
  // Set up the tag table used for exception tag checks.
  //--------------------------------------------------------------------------
  int tags_count = static_cast<int>(module_->tags.size());
  if (tags_count > 0) {
    Handle<FixedArray> tag_table =
        isolate_->factory()->NewFixedArray(tags_count, AllocationType::kOld);
    instance->set_tags_table(*tag_table);
    tags_wrappers_.resize(tags_count);
  }

  //--------------------------------------------------------------------------
  // Set up table storage space.
  //--------------------------------------------------------------------------
  instance->set_isorecursive_canonical_types(
      module_->isorecursive_canonical_type_ids.data());
  int table_count = static_cast<int>(module_->tables.size());
  {
    for (int i = 0; i < table_count; i++) {
      const WasmTable& table = module_->tables[i];
      if (table.initial_size > v8_flags.wasm_max_table_size) {
        thrower_->RangeError(
            "initial table size (%u elements) is larger than implementation "
            "limit (%u elements)",
            table.initial_size, v8_flags.wasm_max_table_size.value());
        return {};
      }
    }

    Handle<FixedArray> tables = isolate_->factory()->NewFixedArray(table_count);
    for (int i = module_->num_imported_tables; i < table_count; i++) {
      const WasmTable& table = module_->tables[i];
      // Initialize tables with null for now. We will initialize non-defaultable
      // tables later, in {SetTableInitialValues}.
      Handle<WasmTableObject> table_obj = WasmTableObject::New(
          isolate_, instance, table.type, table.initial_size,
          table.has_maximum_size, table.maximum_size, nullptr,
          IsSubtypeOf(table.type, kWasmExternRef, module_)
              ? Handle<Object>::cast(isolate_->factory()->null_value())
              : Handle<Object>::cast(isolate_->factory()->wasm_null()));
      tables->set(i, *table_obj);
    }
    instance->set_tables(*tables);
  }

  {
    Handle<FixedArray> tables = isolate_->factory()->NewFixedArray(table_count);
    for (int i = 0; i < table_count; ++i) {
      const WasmTable& table = module_->tables[i];
      if (IsSubtypeOf(table.type, kWasmFuncRef, module_)) {
        Handle<WasmIndirectFunctionTable> table_obj =
            WasmIndirectFunctionTable::New(isolate_, table.initial_size);
        tables->set(i, *table_obj);
      }
    }
    instance->set_indirect_function_tables(*tables);
  }

  instance->SetIndirectFunctionTableShortcuts(isolate_);

  //--------------------------------------------------------------------------
  // Process the imports for the module.
  //--------------------------------------------------------------------------
  if (!module_->import_table.empty()) {
    int num_imported_functions = ProcessImports(instance);
    if (num_imported_functions < 0) return {};
    wasm_module_instantiated.imported_function_count = num_imported_functions;
  }

  //--------------------------------------------------------------------------
  // Create maps for managed objects (GC proposal).
  // Must happen before {InitGlobals} because globals can refer to these maps.
  // We do not need to cache the canonical rtts to (rtt.canon any)'s subtype
  // list.
  //--------------------------------------------------------------------------
  if (enabled_.has_gc()) {
    if (module_->isorecursive_canonical_type_ids.size() > 0) {
      // Make sure all canonical indices have been set.
      DCHECK_NE(module_->MaxCanonicalTypeIndex(), kNoSuperType);
      isolate_->heap()->EnsureWasmCanonicalRttsSize(
          module_->MaxCanonicalTypeIndex() + 1);
    }
    Handle<FixedArray> maps = isolate_->factory()->NewFixedArray(
        static_cast<int>(module_->types.size()));
    for (uint32_t index = 0; index < module_->types.size(); index++) {
      CreateMapForType(isolate_, module_, index, instance, maps);
    }
    instance->set_managed_object_maps(*maps);
  }

  //--------------------------------------------------------------------------
  // Allocate the array that will hold type feedback vectors.
  //--------------------------------------------------------------------------
  if (enabled_.has_inlining()) {
    int num_functions = static_cast<int>(module_->num_declared_functions);
    // Zero-fill the array so we can do a quick Smi-check to test if a given
    // slot was initialized.
    Handle<FixedArray> vectors = isolate_->factory()->NewFixedArrayWithZeroes(
        num_functions, AllocationType::kOld);
    instance->set_feedback_vectors(*vectors);
  }

  //--------------------------------------------------------------------------
  // Process the initialization for the module's globals.
  //--------------------------------------------------------------------------
  InitGlobals(instance);

  //--------------------------------------------------------------------------
  // Initialize the indirect function tables and dispatch tables. We do this
  // before initializing non-defaultable tables and loading element segments, so
  // that indirect function tables in this module are included in the updates
  // when we do so.
  //--------------------------------------------------------------------------
  for (int table_index = 0;
       table_index < static_cast<int>(module_->tables.size()); ++table_index) {
    const WasmTable& table = module_->tables[table_index];

    if (IsSubtypeOf(table.type, kWasmFuncRef, module_)) {
      WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
          instance, table_index, table.initial_size);
      if (thrower_->error()) return {};
      auto table_object =
          handle(WasmTableObject::cast(instance->tables()->get(table_index)),
                 isolate_);
      WasmTableObject::AddDispatchTable(isolate_, table_object, instance,
                                        table_index);
    }
  }

  //--------------------------------------------------------------------------
  // Initialize non-defaultable tables.
  //--------------------------------------------------------------------------
  if (enabled_.has_typed_funcref()) {
    SetTableInitialValues(instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the tags table.
  //--------------------------------------------------------------------------
  if (tags_count > 0) {
    InitializeTags(instance);
  }

  //--------------------------------------------------------------------------
  // Set up the exports object for the new instance.
  //--------------------------------------------------------------------------
  ProcessExports(instance);
  if (thrower_->error()) return {};

  //--------------------------------------------------------------------------
  // Set up uninitialized element segments.
  //--------------------------------------------------------------------------
  if (!module_->elem_segments.empty()) {
    Handle<FixedArray> elements = isolate_->factory()->NewFixedArray(
        static_cast<int>(module_->elem_segments.size()));
    for (int i = 0; i < static_cast<int>(module_->elem_segments.size()); i++) {
      // Initialize declarative segments as empty. The rest remain
      // uninitialized.
      bool is_declarative = module_->elem_segments[i].status ==
                            WasmElemSegment::kStatusDeclarative;
      elements->set(
          i, is_declarative
                 ? Object::cast(*isolate_->factory()->empty_fixed_array())
                 : *isolate_->factory()->undefined_value());
    }
    instance->set_element_segments(*elements);
  }

  //--------------------------------------------------------------------------
  // Load element segments into tables.
  //--------------------------------------------------------------------------
  if (table_count > 0) {
    LoadTableSegments(instance);
    if (thrower_->error()) return {};
  }

  //--------------------------------------------------------------------------
  // Initialize the memory by loading data segments.
  //--------------------------------------------------------------------------
  if (module_->data_segments.size() > 0) {
    LoadDataSegments(instance);
    if (thrower_->error()) return {};
  }

  //--------------------------------------------------------------------------
  // Create a wrapper for the start function.
  //--------------------------------------------------------------------------
  if (module_->start_function_index >= 0) {
    int start_index = module_->start_function_index;
    auto& function = module_->functions[start_index];
    // TODO(clemensb): Don't generate an exported function for the start
    // function. Use CWasmEntry instead.
    Handle<WasmInternalFunction> internal =
        WasmInstanceObject::GetOrCreateWasmInternalFunction(isolate_, instance,
                                                            start_index);
    start_function_ = Handle<WasmExportedFunction>::cast(
        WasmInternalFunction::GetOrCreateExternal(internal));

    if (function.imported) {
      ImportedFunctionEntry entry(instance, module_->start_function_index);
      Tagged<Object> callable = entry.maybe_callable();
      if (IsJSFunction(callable)) {
        // If the start function was imported and calls into Blink, we have
        // to pretend that the V8 API was used to enter its correct context.
        // To get that context to {ExecuteStartFunction} below, we install it
        // as the context of the wrapper we just compiled. That's a bit of a
        // hack because it's not really the wrapper's context, only its wrapped
        // target's context, but the end result is the same, and since the
        // start function wrapper doesn't leak, neither does this
        // implementation detail.
        start_function_->set_context(JSFunction::cast(callable)->context());
      }
    }
  }

  DCHECK(!isolate_->has_pending_exception());
  TRACE("Successfully built instance for module %p\n",
        module_object_->native_module());
  wasm_module_instantiated.success = true;
  if (timer.IsStarted()) {
    base::TimeDelta instantiation_time = timer.Elapsed();
    wasm_module_instantiated.wall_clock_duration_in_us =
        instantiation_time.InMicroseconds();
    SELECT_WASM_COUNTER(isolate_->counters(), module_->origin, wasm_instantiate,
                        module_time)
        ->AddTimedSample(instantiation_time);
    isolate_->metrics_recorder()->DelayMainThreadEvent(wasm_module_instantiated,
                                                       context_id_);
  }
  return instance;
}

bool InstanceBuilder::ExecuteStartFunction() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.ExecuteStartFunction");
  if (start_function_.is_null()) return true;  // No start function.

  HandleScope scope(isolate_);
  // In case the start function calls out to Blink, we have to make sure that
  // the correct "entered context" is available. This is the equivalent of
  // v8::Context::Enter() and must happen in addition to the function call
  // sequence doing the compiled version of "isolate->set_context(...)".
  HandleScopeImplementer* hsi = isolate_->handle_scope_implementer();
  hsi->EnterContext(start_function_->native_context());

  // Call the JS function.
  Handle<Object> undefined = isolate_->factory()->undefined_value();
  MaybeHandle<Object> retval =
      Execution::Call(isolate_, start_function_, undefined, 0, nullptr);
  hsi->LeaveContext();

  if (retval.is_null()) {
    DCHECK(isolate_->has_pending_exception());
    return false;
  }
  return true;
}

// Look up an import value in the {ffi_} object.
MaybeHandle<Object> InstanceBuilder::LookupImport(uint32_t index,
                                                  Handle<String> module_name,
                                                  Handle<String> import_name) {
  // We pre-validated in the js-api layer that the ffi object is present, and
  // a JSObject, if the module has imports.
  DCHECK(!ffi_.is_null());
  // Look up the module first.
  Handle<Object> module;
  if (!Object::GetPropertyOrElement(isolate_, ffi_.ToHandleChecked(),
                                    module_name)
           .ToHandle(&module) ||
      !IsJSReceiver(*module)) {
    const char* error = module.is_null()
                            ? "module not found"
                            : "module is not an object or function";
    thrower_->TypeError("%s: %s", ImportName(index, module_name).c_str(),
                        error);
    return {};
  }

  MaybeHandle<Object> value =
      Object::GetPropertyOrElement(isolate_, module, import_name);
  if (value.is_null()) {
    thrower_->LinkError("%s: import not found",
                        ImportName(index, module_name, import_name).c_str());
    return {};
  }

  return value;
}

namespace {
bool HasDefaultToNumberBehaviour(Isolate* isolate,
                                 Handle<JSFunction> function) {
  // Disallow providing a [Symbol.toPrimitive] member.
  LookupIterator to_primitive_it{isolate, function,
                                 isolate->factory()->to_primitive_symbol()};
  if (to_primitive_it.state() != LookupIterator::NOT_FOUND) return false;

  // The {valueOf} member must be the default "ObjectPrototypeValueOf".
  LookupIterator value_of_it{isolate, function,
                             isolate->factory()->valueOf_string()};
  if (value_of_it.state() != LookupIterator::DATA) return false;
  Handle<Object> value_of = value_of_it.GetDataValue();
  if (!IsJSFunction(*value_of)) return false;
  Builtin value_of_builtin_id =
      Handle<JSFunction>::cast(value_of)->code()->builtin_id();
  if (value_of_builtin_id != Builtin::kObjectPrototypeValueOf) return false;

  // The {toString} member must be the default "FunctionPrototypeToString".
  LookupIterator to_string_it{isolate, function,
                              isolate->factory()->toString_string()};
  if (to_string_it.state() != LookupIterator::DATA) return false;
  Handle<Object> to_string = to_string_it.GetDataValue();
  if (!IsJSFunction(*to_string)) return false;
  Builtin to_string_builtin_id =
      Handle<JSFunction>::cast(to_string)->code()->builtin_id();
  if (to_string_builtin_id != Builtin::kFunctionPrototypeToString) return false;

  // Just a default function, which will convert to "Nan". Accept this.
  return true;
}

bool MaybeMarkError(ValueOrError value, ErrorThrower* thrower) {
  if (is_error(value)) {
    thrower->RuntimeError("%s",
                          MessageFormatter::TemplateString(to_error(value)));
    return true;
  }
  return false;
}
}  // namespace

// Look up an import value in the {ffi_} object specifically for linking an
// asm.js module. This only performs non-observable lookups, which allows
// falling back to JavaScript proper (and hence re-executing all lookups) if
// module instantiation fails.
MaybeHandle<Object> InstanceBuilder::LookupImportAsm(
    uint32_t index, Handle<String> import_name) {
  // Check that a foreign function interface object was provided.
  if (ffi_.is_null()) {
    thrower_->LinkError("%s: missing imports object",
                        ImportName(index, import_name).c_str());
    return {};
  }

  // Perform lookup of the given {import_name} without causing any observable
  // side-effect. We only accept accesses that resolve to data properties,
  // which is indicated by the asm.js spec in section 7 ("Linking") as well.
  PropertyKey key(isolate_, Handle<Name>::cast(import_name));
  LookupIterator it(isolate_, ffi_.ToHandleChecked(), key);
  switch (it.state()) {
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::JSPROXY:
    case LookupIterator::WASM_OBJECT:
    case LookupIterator::ACCESSOR:
    case LookupIterator::TRANSITION:
      thrower_->LinkError("%s: not a data property",
                          ImportName(index, import_name).c_str());
      return {};
    case LookupIterator::NOT_FOUND:
      // Accepting missing properties as undefined does not cause any
      // observable difference from JavaScript semantics, we are lenient.
      return isolate_->factory()->undefined_value();
    case LookupIterator::DATA: {
      Handle<Object> value = it.GetDataValue();
      // For legacy reasons, we accept functions for imported globals (see
      // {ProcessImportedGlobal}), but only if we can easily determine that
      // their Number-conversion is side effect free and returns NaN (which is
      // the case as long as "valueOf" (or others) are not overwritten).
      if (IsJSFunction(*value) &&
          module_->import_table[index].kind == kExternalGlobal &&
          !HasDefaultToNumberBehaviour(isolate_,
                                       Handle<JSFunction>::cast(value))) {
        thrower_->LinkError("%s: function has special ToNumber behaviour",
                            ImportName(index, import_name).c_str());
        return {};
      }
      return value;
    }
  }
}

// Load data segments into the memory.
void InstanceBuilder::LoadDataSegments(Handle<WasmInstanceObject> instance) {
  base::Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t size = segment.source.length();

    // Passive segments are not copied during instantiation.
    if (!segment.active) continue;

    const WasmMemory& dst_memory = module_->memories[segment.memory_index];
    size_t dest_offset;
    if (dst_memory.is_memory64) {
      ValueOrError result = EvaluateConstantExpression(
          &init_expr_zone_, segment.dest_addr, kWasmI64, isolate_, instance);
      if (MaybeMarkError(result, thrower_)) return;
      uint64_t dest_offset_64 = to_value(result).to_u64();

      // Clamp to {std::numeric_limits<size_t>::max()}, which is always an
      // invalid offset.
      DCHECK_GT(std::numeric_limits<size_t>::max(), dst_memory.max_memory_size);
      dest_offset = static_cast<size_t>(std::min(
          dest_offset_64, uint64_t{std::numeric_limits<size_t>::max()}));
    } else {
      ValueOrError result = EvaluateConstantExpression(
          &init_expr_zone_, segment.dest_addr, kWasmI32, isolate_, instance);
      if (MaybeMarkError(result, thrower_)) return;
      dest_offset = to_value(result).to_u32();
    }

    size_t memory_size = instance->memory_size(segment.memory_index);
    if (!base::IsInBounds<size_t>(dest_offset, size, memory_size)) {
      size_t segment_index = &segment - module_->data_segments.data();
      thrower_->RuntimeError(
          "data segment %zu is out of bounds (offset %zu, "
          "length %u, memory size %zu)",
          segment_index, dest_offset, size, memory_size);
      return;
    }

    uint8_t* memory_base = instance->memory_base(segment.memory_index);
    std::memcpy(memory_base + dest_offset,
                wire_bytes.begin() + segment.source.offset(), size);
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global,
                                       const WasmValue& value) {
  TRACE("init [globals_start=%p + %u] = %s, type = %s\n",
        global.type.is_reference()
            ? reinterpret_cast<uint8_t*>(tagged_globals_->address())
            : raw_buffer_ptr(untagged_globals_, 0),
        global.offset, value.to_string().c_str(), global.type.name().c_str());
  DCHECK(IsSubtypeOf(value.type(), global.type, module_));
  if (global.type.is_numeric()) {
    value.CopyTo(GetRawUntaggedGlobalPtr<uint8_t>(global));
  } else {
    tagged_globals_->set(global.offset, *value.to_ref());
  }
}

void InstanceBuilder::SanitizeImports() {
  base::Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (size_t index = 0; index < module_->import_table.size(); ++index) {
    const WasmImport& import = module_->import_table[index];

    Handle<String> module_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate_, wire_bytes, import.module_name, kInternalize);

    Handle<String> import_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate_, wire_bytes, import.field_name, kInternalize);

    int int_index = static_cast<int>(index);
    MaybeHandle<Object> result =
        is_asmjs_module(module_)
            ? LookupImportAsm(int_index, import_name)
            : LookupImport(int_index, module_name, import_name);
    if (thrower_->error()) {
      thrower_->LinkError("Could not find value for import %zu", index);
      return;
    }
    Handle<Object> value = result.ToHandleChecked();
    sanitized_imports_.push_back({module_name, import_name, value});
  }
}

bool InstanceBuilder::ProcessImportedFunction(
    Handle<WasmInstanceObject> instance, int import_index, int func_index,
    Handle<String> module_name, Handle<String> import_name,
    Handle<Object> value) {
  // Function imports must be callable.
  if (!IsCallable(*value)) {
    thrower_->LinkError(
        "%s: function import requires a callable",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }
  // Store any {WasmExternalFunction} callable in the instance before the call
  // is resolved to preserve its identity. This handles exported functions as
  // well as functions constructed via other means (e.g. WebAssembly.Function).
  if (WasmExternalFunction::IsWasmExternalFunction(*value)) {
    WasmInstanceObject::SetWasmInternalFunction(
        instance, func_index,
        WasmInternalFunction::FromExternal(
            Handle<WasmExternalFunction>::cast(value), isolate_)
            .ToHandleChecked());
  }
  auto js_receiver = Handle<JSReceiver>::cast(value);
  const FunctionSig* expected_sig = module_->functions[func_index].sig;
  uint32_t sig_index = module_->functions[func_index].sig_index;
  uint32_t canonical_type_index =
      module_->isorecursive_canonical_type_ids[sig_index];
  WasmImportData resolved(instance, func_index, js_receiver, expected_sig,
                          canonical_type_index);
  if (resolved.well_known_status() != WellKnownImport::kGeneric &&
      v8_flags.trace_wasm_inlining) {
    PrintF("[import %d is well-known built-in %s]\n", import_index,
           WellKnownImportName(resolved.well_known_status()));
  }
  well_known_imports_.push_back(resolved.well_known_status());
  ImportCallKind kind = resolved.kind();
  js_receiver = resolved.callable();
  switch (kind) {
    case ImportCallKind::kLinkError:
      thrower_->LinkError(
          "%s: imported function does not match the expected type",
          ImportName(import_index, module_name, import_name).c_str());
      return false;
    case ImportCallKind::kWasmToWasm: {
      // The imported function is a Wasm function from another instance.
      auto imported_function = Handle<WasmExportedFunction>::cast(js_receiver);
      Handle<WasmInstanceObject> imported_instance(
          imported_function->instance(), isolate_);
      // The import reference is the instance object itself.
      Address imported_target = imported_function->GetWasmCallTarget();
      ImportedFunctionEntry entry(instance, func_index);
      entry.SetWasmToWasm(*imported_instance, imported_target);
      break;
    }
    case ImportCallKind::kWasmToCapi: {
      NativeModule* native_module = instance->module_object()->native_module();
      int expected_arity = static_cast<int>(expected_sig->parameter_count());
      WasmImportWrapperCache* cache = native_module->import_wrapper_cache();
      // TODO(jkummerow): Consider precompiling CapiCallWrappers in parallel,
      // just like other import wrappers.
      uint32_t canonical_type_index =
          module_->isorecursive_canonical_type_ids
              [module_->functions[func_index].sig_index];
      WasmCode* wasm_code = cache->MaybeGet(kind, canonical_type_index,
                                            expected_arity, kNoSuspend);
      if (wasm_code == nullptr) {
        WasmCodeRefScope code_ref_scope;
        WasmImportWrapperCache::ModificationScope cache_scope(cache);
        wasm_code =
            compiler::CompileWasmCapiCallWrapper(native_module, expected_sig);
        WasmImportWrapperCache::CacheKey key(kind, canonical_type_index,
                                             expected_arity, kNoSuspend);
        cache_scope[key] = wasm_code;
        wasm_code->IncRef();
        isolate_->counters()->wasm_generated_code_size()->Increment(
            wasm_code->instructions().length());
        isolate_->counters()->wasm_reloc_size()->Increment(
            wasm_code->reloc_info().length());
      }

      ImportedFunctionEntry entry(instance, func_index);
      // We re-use the SetWasmToJs infrastructure because it passes the
      // callable to the wrapper, which we need to get the function data.
      entry.SetWasmToJs(isolate_, js_receiver, wasm_code, kNoSuspend,
                        expected_sig);
      break;
    }
    case ImportCallKind::kWasmToJSFastApi: {
      NativeModule* native_module = instance->module_object()->native_module();
      DCHECK(IsJSFunction(*js_receiver) || IsJSBoundFunction(*js_receiver));
      WasmCodeRefScope code_ref_scope;
      WasmCode* wasm_code = compiler::CompileWasmJSFastCallWrapper(
          native_module, expected_sig, js_receiver);
      ImportedFunctionEntry entry(instance, func_index);
      entry.SetWasmToJs(isolate_, js_receiver, wasm_code, kNoSuspend,
                        expected_sig);
      break;
    }
    default: {
      // The imported function is a callable.
      if (UseGenericWasmToJSWrapper(kind, expected_sig, resolved.suspend()) &&
          (kind == ImportCallKind::kJSFunctionArityMatch ||
           kind == ImportCallKind::kJSFunctionArityMismatch)) {
        ImportedFunctionEntry entry(instance, func_index);
        entry.SetWasmToJs(isolate_, js_receiver, resolved.suspend(),
                          expected_sig);
        break;
      }
      int expected_arity = static_cast<int>(expected_sig->parameter_count());
      if (kind == ImportCallKind::kJSFunctionArityMismatch) {
        Handle<JSFunction> function = Handle<JSFunction>::cast(js_receiver);
        Tagged<SharedFunctionInfo> shared = function->shared();
        expected_arity =
            shared->internal_formal_parameter_count_without_receiver();
      }

      NativeModule* native_module = instance->module_object()->native_module();
      uint32_t canonical_type_index =
          module_->isorecursive_canonical_type_ids
              [module_->functions[func_index].sig_index];
      WasmCode* wasm_code = native_module->import_wrapper_cache()->Get(
          kind, canonical_type_index, expected_arity, resolved.suspend());
      DCHECK_NOT_NULL(wasm_code);
      ImportedFunctionEntry entry(instance, func_index);
      if (wasm_code->kind() == WasmCode::kWasmToJsWrapper) {
        // Wasm to JS wrappers are treated specially in the import table.
        entry.SetWasmToJs(isolate_, js_receiver, wasm_code, resolved.suspend(),
                          expected_sig);
      } else {
        // Wasm math intrinsics are compiled as regular Wasm functions.
        DCHECK(kind >= ImportCallKind::kFirstMathIntrinsic &&
               kind <= ImportCallKind::kLastMathIntrinsic);
        entry.SetWasmToWasm(*instance, wasm_code->instruction_start());
      }
      break;
    }
  }
  return true;
}

bool InstanceBuilder::InitializeImportedIndirectFunctionTable(
    Handle<WasmInstanceObject> instance, int table_index, int import_index,
    Handle<WasmTableObject> table_object) {
  int imported_table_size = table_object->current_length();
  // Allocate a new dispatch table.
  WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
      instance, table_index, imported_table_size);
  // Initialize the dispatch table with the (foreign) JS functions
  // that are already in the table.
  for (int i = 0; i < imported_table_size; ++i) {
    bool is_valid;
    bool is_null;
    MaybeHandle<WasmInstanceObject> maybe_target_instance;
    int function_index;
    MaybeHandle<WasmJSFunction> maybe_js_function;
    WasmTableObject::GetFunctionTableEntry(
        isolate_, module_, table_object, i, &is_valid, &is_null,
        &maybe_target_instance, &function_index, &maybe_js_function);
    if (!is_valid) {
      thrower_->LinkError("table import %d[%d] is not a wasm function",
                          import_index, i);
      return false;
    }
    if (is_null) continue;
    Handle<WasmJSFunction> js_function;
    if (maybe_js_function.ToHandle(&js_function)) {
      WasmInstanceObject::ImportWasmJSFunctionIntoTable(
          isolate_, instance, table_index, i, js_function);
      continue;
    }

    Handle<WasmInstanceObject> target_instance =
        maybe_target_instance.ToHandleChecked();
    const WasmModule* target_module =
        target_instance->module_object()->module();
    const WasmFunction& function = target_module->functions[function_index];

    FunctionTargetAndRef entry(target_instance, function_index);
    Handle<Object> ref = entry.ref();
    if (v8_flags.wasm_to_js_generic_wrapper && IsWasmApiFunctionRef(*ref)) {
      Handle<WasmApiFunctionRef> orig_ref =
          Handle<WasmApiFunctionRef>::cast(ref);
      Handle<WasmApiFunctionRef> new_ref =
          isolate_->factory()->NewWasmApiFunctionRef(orig_ref);
      WasmApiFunctionRef::SetCrossInstanceTableIndexAsCallOrigin(
          isolate_, new_ref, instance, i);
      ref = new_ref;
    }

    uint32_t canonicalized_sig_index =
        target_module->isorecursive_canonical_type_ids[function.sig_index];

    instance->GetIndirectFunctionTable(isolate_, table_index)
        ->Set(i, canonicalized_sig_index, entry.call_target(), *ref);
  }
  return true;
}

bool InstanceBuilder::ProcessImportedTable(Handle<WasmInstanceObject> instance,
                                           int import_index, int table_index,
                                           Handle<String> module_name,
                                           Handle<String> import_name,
                                           Handle<Object> value) {
  if (!IsWasmTableObject(*value)) {
    thrower_->LinkError(
        "%s: table import requires a WebAssembly.Table",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }
  const WasmTable& table = module_->tables[table_index];

  auto table_object = Handle<WasmTableObject>::cast(value);

  uint32_t imported_table_size =
      static_cast<uint32_t>(table_object->current_length());
  if (imported_table_size < table.initial_size) {
    thrower_->LinkError("table import %d is smaller than initial %u, got %u",
                        import_index, table.initial_size, imported_table_size);
    return false;
  }

  if (table.has_maximum_size) {
    if (IsUndefined(table_object->maximum_length(), isolate_)) {
      thrower_->LinkError("table import %d has no maximum length, expected %u",
                          import_index, table.maximum_size);
      return false;
    }
    int64_t imported_maximum_size =
        Object::Number(table_object->maximum_length());
    if (imported_maximum_size < 0) {
      thrower_->LinkError("table import %d has no maximum length, expected %u",
                          import_index, table.maximum_size);
      return false;
    }
    if (imported_maximum_size > table.maximum_size) {
      thrower_->LinkError("table import %d has a larger maximum size %" PRIx64
                          " than the module's declared maximum %u",
                          import_index, imported_maximum_size,
                          table.maximum_size);
      return false;
    }
  }

  const WasmModule* table_type_module =
      !IsUndefined(table_object->instance())
          ? WasmInstanceObject::cast(table_object->instance())->module()
          : instance->module();

  if (!EquivalentTypes(table.type, table_object->type(), module_,
                       table_type_module)) {
    thrower_->LinkError(
        "%s: imported table does not match the expected type",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }

  if (IsSubtypeOf(table.type, kWasmFuncRef, module_) &&
      !InitializeImportedIndirectFunctionTable(instance, table_index,
                                               import_index, table_object)) {
    return false;
  }

  instance->tables()->set(table_index, *value);
  return true;
}

bool InstanceBuilder::ProcessImportedWasmGlobalObject(
    Handle<WasmInstanceObject> instance, int import_index,
    Handle<String> module_name, Handle<String> import_name,
    const WasmGlobal& global, Handle<WasmGlobalObject> global_object) {
  if (static_cast<bool>(global_object->is_mutable()) != global.mutability) {
    thrower_->LinkError(
        "%s: imported global does not match the expected mutability",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }

  const WasmModule* global_type_module =
      !IsUndefined(global_object->instance())
          ? WasmInstanceObject::cast(global_object->instance())->module()
          : instance->module();

  bool valid_type =
      global.mutability
          ? EquivalentTypes(global_object->type(), global.type,
                            global_type_module, instance->module())
          : IsSubtypeOf(global_object->type(), global.type, global_type_module,
                        instance->module());

  if (!valid_type) {
    thrower_->LinkError(
        "%s: imported global does not match the expected type",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }
  if (global.mutability) {
    DCHECK_LT(global.index, module_->num_imported_mutable_globals);
    Handle<Object> buffer;
    if (global.type.is_reference()) {
      static_assert(sizeof(global_object->offset()) <= sizeof(Address),
                    "The offset into the globals buffer does not fit into "
                    "the imported_mutable_globals array");
      buffer = handle(global_object->tagged_buffer(), isolate_);
      // For externref globals we use a relative offset, not an absolute
      // address.
      instance->imported_mutable_globals()->set(global.index,
                                                global_object->offset());
    } else {
      buffer = handle(global_object->untagged_buffer(), isolate_);
      // It is safe in this case to store the raw pointer to the buffer
      // since the backing store of the JSArrayBuffer will not be
      // relocated.
      Address address = reinterpret_cast<Address>(raw_buffer_ptr(
          Handle<JSArrayBuffer>::cast(buffer), global_object->offset()));
      instance->imported_mutable_globals()->set_sandboxed_pointer(global.index,
                                                                  address);
    }
    instance->imported_mutable_globals_buffers()->set(global.index, *buffer);
    return true;
  }

  WasmValue value;
  switch (global_object->type().kind()) {
    case kI32:
      value = WasmValue(global_object->GetI32());
      break;
    case kI64:
      value = WasmValue(global_object->GetI64());
      break;
    case kF32:
      value = WasmValue(global_object->GetF32());
      break;
    case kF64:
      value = WasmValue(global_object->GetF64());
      break;
    case kS128:
      value = WasmValue(global_object->GetS128RawBytes(), kWasmS128);
      break;
    case kRef:
    case kRefNull:
      value = WasmValue(global_object->GetRef(), global_object->type());
      break;
    case kVoid:
    case kBottom:
    case kRtt:
    case kI8:
    case kI16:
      UNREACHABLE();
  }

  WriteGlobalValue(global, value);
  return true;
}

bool InstanceBuilder::ProcessImportedGlobal(Handle<WasmInstanceObject> instance,
                                            int import_index, int global_index,
                                            Handle<String> module_name,
                                            Handle<String> import_name,
                                            Handle<Object> value) {
  // Immutable global imports are converted to numbers and written into
  // the {untagged_globals_} array buffer.
  //
  // Mutable global imports instead have their backing array buffers
  // referenced by this instance, and store the address of the imported
  // global in the {imported_mutable_globals_} array.
  const WasmGlobal& global = module_->globals[global_index];

  // SIMD proposal allows modules to define an imported v128 global, and only
  // supports importing a WebAssembly.Global object for this global, but also
  // defines constructing a WebAssembly.Global of v128 to be a TypeError.
  // We *should* never hit this case in the JS API, but the module should should
  // be allowed to declare such a global (no validation error).
  if (global.type == kWasmS128 && !IsWasmGlobalObject(*value)) {
    thrower_->LinkError(
        "%s: global import of type v128 must be a WebAssembly.Global",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }

  if (is_asmjs_module(module_)) {
    // Accepting {JSFunction} on top of just primitive values here is a
    // workaround to support legacy asm.js code with broken binding. Note
    // that using {NaN} (or Smi::zero()) here is what using the observable
    // conversion via {ToPrimitive} would produce as well. {LookupImportAsm}
    // checked via {HasDefaultToNumberBehaviour} that "valueOf" or friends have
    // not been patched.
    if (IsJSFunction(*value)) value = isolate_->factory()->nan_value();
    if (IsPrimitive(*value)) {
      MaybeHandle<Object> converted = global.type == kWasmI32
                                          ? Object::ToInt32(isolate_, value)
                                          : Object::ToNumber(isolate_, value);
      if (!converted.ToHandle(&value)) {
        // Conversion is known to fail for Symbols and BigInts.
        thrower_->LinkError(
            "%s: global import must be a number",
            ImportName(import_index, module_name, import_name).c_str());
        return false;
      }
    }
  }

  if (IsWasmGlobalObject(*value)) {
    auto global_object = Handle<WasmGlobalObject>::cast(value);
    return ProcessImportedWasmGlobalObject(instance, import_index, module_name,
                                           import_name, global, global_object);
  }

  if (global.mutability) {
    thrower_->LinkError(
        "%s: imported mutable global must be a WebAssembly.Global object",
        ImportName(import_index, module_name, import_name).c_str());
    return false;
  }

  if (global.type.is_reference()) {
    const char* error_message;
    Handle<Object> wasm_value;
    if (!wasm::JSToWasmObject(isolate_, module_, value, global.type,
                              &error_message)
             .ToHandle(&wasm_value)) {
      thrower_->LinkError(
          "%s: %s", ImportName(global_index, module_name, import_name).c_str(),
          error_message);
      return false;
    }
    WriteGlobalValue(global, WasmValue(wasm_value, global.type));
    return true;
  }

  if (IsNumber(*value) && global.type != kWasmI64) {
    double number_value = Object::Number(*value);
    // The Wasm-BigInt proposal currently says that i64 globals may
    // only be initialized with BigInts. See:
    // https://github.com/WebAssembly/JS-BigInt-integration/issues/12
    WasmValue wasm_value = global.type == kWasmI32
                               ? WasmValue(DoubleToInt32(number_value))
                               : global.type == kWasmF32
                                     ? WasmValue(DoubleToFloat32(number_value))
                                     : WasmValue(number_value);
    WriteGlobalValue(global, wasm_value);
    return true;
  }

  if (global.type == kWasmI64 && IsBigInt(*value)) {
    WriteGlobalValue(global, WasmValue(BigInt::cast(*value)->AsInt64()));
    return true;
  }

  thrower_->LinkError(
      "%s: global import must be a number, valid Wasm reference, or "
      "WebAssembly.Global object",
      ImportName(import_index, module_name, import_name).c_str());
  return false;
}

void InstanceBuilder::CompileImportWrappers(
    Handle<WasmInstanceObject> instance) {
  int num_imports = static_cast<int>(module_->import_table.size());
  TRACE_EVENT1("v8.wasm", "wasm.CompileImportWrappers", "num_imports",
               num_imports);
  NativeModule* native_module = instance->module_object()->native_module();
  WasmImportWrapperCache::ModificationScope cache_scope(
      native_module->import_wrapper_cache());

  // Compilation is done in two steps:
  // 1) Insert nullptr entries in the cache for wrappers that need to be
  // compiled. 2) Compile wrappers in background tasks using the
  // ImportWrapperQueue. This way the cache won't invalidate other iterators
  // when inserting a new WasmCode, since the key will already be there.
  ImportWrapperQueue import_wrapper_queue;
  for (int index = 0; index < num_imports; ++index) {
    Handle<Object> value = sanitized_imports_[index].value;
    if (module_->import_table[index].kind != kExternalFunction ||
        !IsCallable(*value)) {
      continue;
    }
    auto js_receiver = Handle<JSReceiver>::cast(value);
    uint32_t func_index = module_->import_table[index].index;
    const FunctionSig* sig = module_->functions[func_index].sig;
    uint32_t sig_index = module_->functions[func_index].sig_index;
    uint32_t canonical_type_index =
        module_->isorecursive_canonical_type_ids[sig_index];
    WasmImportData resolved({}, func_index, js_receiver, sig,
                            canonical_type_index);
    if (UseGenericWasmToJSWrapper(resolved.kind(), sig, resolved.suspend())) {
      continue;
    }
    ImportCallKind kind = resolved.kind();
    if (kind == ImportCallKind::kWasmToWasm ||
        kind == ImportCallKind::kLinkError ||
        kind == ImportCallKind::kWasmToCapi ||
        kind == ImportCallKind::kWasmToJSFastApi) {
      continue;
    }

    int expected_arity = static_cast<int>(sig->parameter_count());
    if (kind == ImportCallKind::kJSFunctionArityMismatch) {
      Handle<JSFunction> function =
          Handle<JSFunction>::cast(resolved.callable());
      Tagged<SharedFunctionInfo> shared = function->shared();
      expected_arity =
          shared->internal_formal_parameter_count_without_receiver();
    }
    WasmImportWrapperCache::CacheKey key(kind, canonical_type_index,
                                         expected_arity, resolved.suspend());
    if (cache_scope[key] != nullptr) {
      // Cache entry already exists, no need to compile it again.
      continue;
    }
    import_wrapper_queue.insert(key, sig);
  }

  auto compile_job_task = std::make_unique<CompileImportWrapperJob>(
      isolate_->counters(), native_module, &import_wrapper_queue, &cache_scope);
  auto compile_job = V8::GetCurrentPlatform()->CreateJob(
      TaskPriority::kUserVisible, std::move(compile_job_task));

  // Wait for the job to finish, while contributing in this thread.
  compile_job->Join();
}

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;

  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());

  CompileImportWrappers(instance);
  int num_imports = static_cast<int>(module_->import_table.size());
  for (int index = 0; index < num_imports; ++index) {
    const WasmImport& import = module_->import_table[index];

    Handle<String> module_name = sanitized_imports_[index].module_name;
    Handle<String> import_name = sanitized_imports_[index].import_name;
    Handle<Object> value = sanitized_imports_[index].value;

    switch (import.kind) {
      case kExternalFunction: {
        uint32_t func_index = import.index;
        DCHECK_EQ(num_imported_functions, func_index);
        if (!ProcessImportedFunction(instance, index, func_index, module_name,
                                     import_name, value)) {
          return -1;
        }
        num_imported_functions++;
        break;
      }
      case kExternalTable: {
        uint32_t table_index = import.index;
        DCHECK_EQ(table_index, num_imported_tables);
        if (!ProcessImportedTable(instance, index, table_index, module_name,
                                  import_name, value)) {
          return -1;
        }
        num_imported_tables++;
        USE(num_imported_tables);
        break;
      }
      case kExternalMemory:
        // Imported memories are already handled earlier via
        // {ProcessImportedMemories}.
        break;
      case kExternalGlobal: {
        if (!ProcessImportedGlobal(instance, index, import.index, module_name,
                                   import_name, value)) {
          return -1;
        }
        break;
      }
      case kExternalTag: {
        if (!IsWasmTagObject(*value)) {
          thrower_->LinkError(
              "%s: tag import requires a WebAssembly.Tag",
              ImportName(index, module_name, import_name).c_str());
          return -1;
        }
        Handle<WasmTagObject> imported_tag = Handle<WasmTagObject>::cast(value);
        if (!imported_tag->MatchesSignature(
                module_->isorecursive_canonical_type_ids
                    [module_->tags[import.index].sig_index])) {
          thrower_->LinkError(
              "%s: imported tag does not match the expected type",
              ImportName(index, module_name, import_name).c_str());
          return -1;
        }
        Tagged<Object> tag = imported_tag->tag();
        DCHECK(IsUndefined(instance->tags_table()->get(import.index)));
        instance->tags_table()->set(import.index, tag);
        tags_wrappers_[import.index] = imported_tag;
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  if (num_imported_functions > 0) {
    WellKnownImportsList::UpdateResult result =
        module_->type_feedback.well_known_imports.Update(
            base::VectorOf(well_known_imports_));
    if (result == WellKnownImportsList::UpdateResult::kFoundIncompatibility) {
      module_object_->native_module()->RemoveCompiledCode(
          NativeModule::RemoveFilter::kRemoveTurbofanCode);
    }
  }
  return num_imported_functions;
}

bool InstanceBuilder::ProcessImportedMemories(
    Handle<FixedArray> imported_memory_objects) {
  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());

  int num_imports = static_cast<int>(module_->import_table.size());
  for (int import_index = 0; import_index < num_imports; ++import_index) {
    const WasmImport& import = module_->import_table[import_index];

    if (import.kind != kExternalMemory) continue;

    Handle<String> module_name = sanitized_imports_[import_index].module_name;
    Handle<String> import_name = sanitized_imports_[import_index].import_name;
    Handle<Object> value = sanitized_imports_[import_index].value;

    if (!IsWasmMemoryObject(*value)) {
      thrower_->LinkError(
          "%s: memory import must be a WebAssembly.Memory object",
          ImportName(import_index, module_name, import_name).c_str());
      return false;
    }
    uint32_t memory_index = import.index;
    auto memory_object = Handle<WasmMemoryObject>::cast(value);

    Handle<JSArrayBuffer> buffer{memory_object->array_buffer(), isolate_};
    uint32_t imported_cur_pages =
        static_cast<uint32_t>(buffer->byte_length() / kWasmPageSize);
    const WasmMemory* memory = &module_->memories[memory_index];
    if (memory->is_memory64 != memory_object->is_memory64()) {
      // For now, we forbid importing memory32 as memory64 and vice versa.
      // TODO(13780): Check if the final spec says anything about this or has
      // any tests. Adapt if needed.
      thrower_->LinkError("cannot import memory%d as memory%d",
                          memory_object->is_memory64() ? 64 : 32,
                          memory->is_memory64 ? 64 : 32);
      return false;
    }
    if (imported_cur_pages < memory->initial_pages) {
      thrower_->LinkError(
          "%s: memory import has %u pages which is smaller than the declared "
          "initial of %u",
          ImportName(import_index, module_name, import_name).c_str(),
          imported_cur_pages, memory->initial_pages);
      return false;
    }
    int32_t imported_maximum_pages = memory_object->maximum_pages();
    if (memory->has_maximum_pages) {
      if (imported_maximum_pages < 0) {
        thrower_->LinkError(
            "%s: memory import has no maximum limit, expected at most %u",
            ImportName(import_index, module_name, import_name).c_str(),
            imported_maximum_pages);
        return false;
      }
      if (static_cast<uint32_t>(imported_maximum_pages) >
          memory->maximum_pages) {
        thrower_->LinkError(
            "%s: memory import has a larger maximum size %u than the "
            "module's declared maximum %u",
            ImportName(import_index, module_name, import_name).c_str(),
            imported_maximum_pages, memory->maximum_pages);
        return false;
      }
    }
    if (memory->is_shared != buffer->is_shared()) {
      thrower_->LinkError(
          "%s: mismatch in shared state of memory, declared = %d, imported = "
          "%d",
          ImportName(import_index, module_name, import_name).c_str(),
          memory->is_shared, buffer->is_shared());
      return false;
    }

    DCHECK_EQ(ReadOnlyRoots{isolate_}.undefined_value(),
              imported_memory_objects->get(memory_index));
    imported_memory_objects->set(memory_index, *memory_object);
  }
  return true;
}

template <typename T>
T* InstanceBuilder::GetRawUntaggedGlobalPtr(const WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(untagged_globals_, global.offset));
}

// Process initialization of globals.
void InstanceBuilder::InitGlobals(Handle<WasmInstanceObject> instance) {
  for (const WasmGlobal& global : module_->globals) {
    if (global.mutability && global.imported) continue;
    // Happens with imported globals.
    if (!global.init.is_set()) continue;

    ValueOrError result = EvaluateConstantExpression(
        &init_expr_zone_, global.init, global.type, isolate_, instance);
    if (MaybeMarkError(result, thrower_)) return;

    if (global.type.is_reference()) {
      tagged_globals_->set(global.offset, *to_value(result).to_ref());
    } else {
      to_value(result).CopyTo(GetRawUntaggedGlobalPtr<uint8_t>(global));
    }
  }
}

// Allocate memory for a module instance as a new JSArrayBuffer.
MaybeHandle<WasmMemoryObject> InstanceBuilder::AllocateMemory(
    uint32_t memory_index) {
  const WasmMemory& memory = module_->memories[memory_index];
  int initial_pages = static_cast<int>(memory.initial_pages);
  int maximum_pages = memory.has_maximum_pages
                          ? static_cast<int>(memory.maximum_pages)
                          : WasmMemoryObject::kNoMaximum;
  auto shared = memory.is_shared ? SharedFlag::kShared : SharedFlag::kNotShared;

  auto mem_type = memory.is_memory64 ? WasmMemoryFlag::kWasmMemory64
                                     : WasmMemoryFlag::kWasmMemory32;
  MaybeHandle<WasmMemoryObject> maybe_memory_object = WasmMemoryObject::New(
      isolate_, initial_pages, maximum_pages, shared, mem_type);
  if (maybe_memory_object.is_null()) {
    thrower_->RangeError(
        "Out of memory: Cannot allocate Wasm memory for new instance");
    return {};
  }
  return maybe_memory_object;
}

// Process the exports, creating wrappers for functions, tables, memories,
// globals, and exceptions.
void InstanceBuilder::ProcessExports(Handle<WasmInstanceObject> instance) {
  std::unordered_map<int, Handle<Object>> imported_globals;

  // If an imported WebAssembly function or global gets exported, the export
  // has to be identical to to import. Therefore we cache all imported
  // WebAssembly functions in the instance, and all imported globals in a map
  // here.
  for (int index = 0, end = static_cast<int>(module_->import_table.size());
       index < end; ++index) {
    const WasmImport& import = module_->import_table[index];
    if (import.kind == kExternalFunction) {
      Handle<Object> value = sanitized_imports_[index].value;
      if (WasmExternalFunction::IsWasmExternalFunction(*value)) {
        WasmInstanceObject::SetWasmInternalFunction(
            instance, import.index,
            WasmInternalFunction::FromExternal(
                Handle<WasmExternalFunction>::cast(value), isolate_)
                .ToHandleChecked());
      }
    } else if (import.kind == kExternalGlobal) {
      Handle<Object> value = sanitized_imports_[index].value;
      if (IsWasmGlobalObject(*value)) {
        imported_globals[import.index] = value;
      }
    }
  }

  Handle<JSObject> exports_object;
  MaybeHandle<String> single_function_name;
  bool is_asm_js = is_asmjs_module(module_);
  if (is_asm_js) {
    Handle<JSFunction> object_function = Handle<JSFunction>(
        isolate_->native_context()->object_function(), isolate_);
    exports_object = isolate_->factory()->NewJSObject(object_function);
    single_function_name =
        isolate_->factory()->InternalizeUtf8String(AsmJs::kSingleFunctionName);
  } else {
    exports_object = isolate_->factory()->NewJSObjectWithNullProto();
  }
  instance->set_exports_object(*exports_object);

  PropertyDescriptor desc;
  desc.set_writable(is_asm_js);
  desc.set_enumerable(true);
  desc.set_configurable(is_asm_js);

  // Process each export in the export table.
  for (const WasmExport& exp : module_->export_table) {
    Handle<String> name = WasmModuleObject::ExtractUtf8StringFromModuleBytes(
        isolate_, module_object_, exp.name, kInternalize);
    Handle<JSObject> export_to = exports_object;
    switch (exp.kind) {
      case kExternalFunction: {
        // Wrap and export the code as a JSFunction.
        Handle<WasmInternalFunction> internal =
            WasmInstanceObject::GetOrCreateWasmInternalFunction(
                isolate_, instance, exp.index);
        Handle<JSFunction> wasm_external_function =
            WasmInternalFunction::GetOrCreateExternal(internal);
        desc.set_value(wasm_external_function);

        if (is_asm_js &&
            String::Equals(isolate_, name,
                           single_function_name.ToHandleChecked())) {
          export_to = instance;
        }
        break;
      }
      case kExternalTable: {
        desc.set_value(handle(instance->tables()->get(exp.index), isolate_));
        break;
      }
      case kExternalMemory: {
        // Export the memory as a WebAssembly.Memory object. A WasmMemoryObject
        // should already be available if the module has memory, since we always
        // create or import it when building an WasmInstanceObject.
        desc.set_value(handle(instance->memory_object(exp.index), isolate_));
        break;
      }
      case kExternalGlobal: {
        const WasmGlobal& global = module_->globals[exp.index];
        if (global.imported) {
          auto cached_global = imported_globals.find(exp.index);
          if (cached_global != imported_globals.end()) {
            desc.set_value(cached_global->second);
            break;
          }
        }
        Handle<JSArrayBuffer> untagged_buffer;
        Handle<FixedArray> tagged_buffer;
        uint32_t offset;

        if (global.mutability && global.imported) {
          Handle<FixedArray> buffers_array(
              instance->imported_mutable_globals_buffers(), isolate_);
          if (global.type.is_reference()) {
            tagged_buffer = handle(
                FixedArray::cast(buffers_array->get(global.index)), isolate_);
            // For externref globals we store the relative offset in the
            // imported_mutable_globals array instead of an absolute address.
            offset = static_cast<uint32_t>(
                instance->imported_mutable_globals()->get(global.index));
          } else {
            untagged_buffer =
                handle(JSArrayBuffer::cast(buffers_array->get(global.index)),
                       isolate_);
            Address global_addr =
                instance->imported_mutable_globals()->get_sandboxed_pointer(
                    global.index);

            size_t buffer_size = untagged_buffer->byte_length();
            Address backing_store =
                reinterpret_cast<Address>(untagged_buffer->backing_store());
            CHECK(global_addr >= backing_store &&
                  global_addr < backing_store + buffer_size);
            offset = static_cast<uint32_t>(global_addr - backing_store);
          }
        } else {
          if (global.type.is_reference()) {
            tagged_buffer = handle(instance->tagged_globals_buffer(), isolate_);
          } else {
            untagged_buffer =
                handle(instance->untagged_globals_buffer(), isolate_);
          }
          offset = global.offset;
        }

        // Since the global's array untagged_buffer is always provided,
        // allocation should never fail.
        Handle<WasmGlobalObject> global_obj =
            WasmGlobalObject::New(isolate_, instance, untagged_buffer,
                                  tagged_buffer, global.type, offset,
                                  global.mutability)
                .ToHandleChecked();
        desc.set_value(global_obj);
        break;
      }
      case kExternalTag: {
        const WasmTag& tag = module_->tags[exp.index];
        Handle<WasmTagObject> wrapper = tags_wrappers_[exp.index];
        if (wrapper.is_null()) {
          Handle<HeapObject> tag_object(
              HeapObject::cast(instance->tags_table()->get(exp.index)),
              isolate_);
          uint32_t canonical_sig_index =
              module_->isorecursive_canonical_type_ids[tag.sig_index];
          wrapper = WasmTagObject::New(isolate_, tag.sig, canonical_sig_index,
                                       tag_object);
          tags_wrappers_[exp.index] = wrapper;
        }
        desc.set_value(wrapper);
        break;
      }
      default:
        UNREACHABLE();
    }

    CHECK(JSReceiver::DefineOwnProperty(isolate_, export_to, name, &desc,
                                        Just(kThrowOnError))
              .FromMaybe(false));
  }

  if (module_->origin == kWasmOrigin) {
    CHECK(JSReceiver::SetIntegrityLevel(isolate_, exports_object, FROZEN,
                                        kDontThrow)
              .FromMaybe(false));
  }
}

namespace {
V8_INLINE void SetFunctionTablePlaceholder(Isolate* isolate,
                                           Handle<WasmInstanceObject> instance,
                                           Handle<WasmTableObject> table_object,
                                           uint32_t entry_index,
                                           uint32_t func_index) {
  const WasmModule* module = instance->module();
  const WasmFunction* function = &module->functions[func_index];
  MaybeHandle<WasmInternalFunction> wasm_internal_function =
      WasmInstanceObject::GetWasmInternalFunction(isolate, instance,
                                                  func_index);
  if (wasm_internal_function.is_null()) {
    // No JSFunction entry yet exists for this function. Create a {Tuple2}
    // holding the information to lazily allocate one.
    WasmTableObject::SetFunctionTablePlaceholder(
        isolate, table_object, entry_index, instance, func_index);
  } else {
    table_object->entries()->set(entry_index,
                                 *wasm_internal_function.ToHandleChecked());
  }
  WasmTableObject::UpdateDispatchTables(isolate, table_object, entry_index,
                                        function, instance);
}

V8_INLINE void SetFunctionTableNullEntry(Isolate* isolate,
                                         Handle<WasmTableObject> table_object,
                                         uint32_t entry_index) {
  table_object->entries()->set(entry_index, *isolate->factory()->wasm_null());
  WasmTableObject::ClearDispatchTables(isolate, table_object, entry_index);
}
}  // namespace

void InstanceBuilder::SetTableInitialValues(
    Handle<WasmInstanceObject> instance) {
  for (int table_index = 0;
       table_index < static_cast<int>(module_->tables.size()); ++table_index) {
    const WasmTable& table = module_->tables[table_index];
    if (table.initial_value.is_set()) {
      auto table_object =
          handle(WasmTableObject::cast(instance->tables()->get(table_index)),
                 isolate_);
      bool is_function_table = IsSubtypeOf(table.type, kWasmFuncRef, module_);
      if (is_function_table &&
          table.initial_value.kind() == ConstantExpression::kRefFunc) {
        for (uint32_t entry_index = 0; entry_index < table.initial_size;
             entry_index++) {
          SetFunctionTablePlaceholder(isolate_, instance, table_object,
                                      entry_index, table.initial_value.index());
        }
      } else if (is_function_table &&
                 table.initial_value.kind() == ConstantExpression::kRefNull) {
        for (uint32_t entry_index = 0; entry_index < table.initial_size;
             entry_index++) {
          SetFunctionTableNullEntry(isolate_, table_object, entry_index);
        }
      } else {
        ValueOrError result =
            EvaluateConstantExpression(&init_expr_zone_, table.initial_value,
                                       table.type, isolate_, instance);
        if (MaybeMarkError(result, thrower_)) return;
        for (uint32_t entry_index = 0; entry_index < table.initial_size;
             entry_index++) {
          WasmTableObject::Set(isolate_, table_object, entry_index,
                               to_value(result).to_ref());
        }
      }
    }
  }
}

namespace {

enum FunctionComputationMode { kLazyFunctionsAndNull, kStrictFunctionsAndNull };

// If {function_mode == kLazyFunctionsAndNull}, may return a function index
// instead of computing a function object, and {WasmValue(-1)} instead of null.
// Assumes the underlying module is verified.
ValueOrError ConsumeElementSegmentEntry(Zone* zone, Isolate* isolate,
                                        Handle<WasmInstanceObject> instance,
                                        const WasmElemSegment& segment,
                                        Decoder& decoder,
                                        FunctionComputationMode function_mode) {
  if (segment.element_type == WasmElemSegment::kFunctionIndexElements) {
    uint32_t function_index = decoder.consume_u32v();
    return function_mode == kStrictFunctionsAndNull
               ? EvaluateConstantExpression(
                     zone, ConstantExpression::RefFunc(function_index),
                     segment.type, isolate, instance)
               : ValueOrError(WasmValue(function_index));
  }

  switch (static_cast<WasmOpcode>(*decoder.pc())) {
    case kExprRefFunc: {
      auto [function_index, length] =
          decoder.read_u32v<Decoder::FullValidationTag>(decoder.pc() + 1,
                                                        "ref.func");
      if (V8_LIKELY(decoder.lookahead(1 + length, kExprEnd))) {
        decoder.consume_bytes(length + 2);
        return function_mode == kStrictFunctionsAndNull
                   ? EvaluateConstantExpression(
                         zone, ConstantExpression::RefFunc(function_index),
                         segment.type, isolate, instance)
                   : ValueOrError(WasmValue(function_index));
      }
      break;
    }
    case kExprRefNull: {
      auto [heap_type, length] =
          value_type_reader::read_heap_type<Decoder::FullValidationTag>(
              &decoder, decoder.pc() + 1, WasmFeatures::All());
      if (V8_LIKELY(decoder.lookahead(1 + length, kExprEnd))) {
        decoder.consume_bytes(length + 2);
        return function_mode == kStrictFunctionsAndNull
                   ? EvaluateConstantExpression(zone,
                                                ConstantExpression::RefNull(
                                                    heap_type.representation()),
                                                segment.type, isolate, instance)
                   : WasmValue(int32_t{-1});
      }
      break;
    }
    default:
      break;
  }

  auto sig = FixedSizeSignature<ValueType>::Returns(segment.type);
  FunctionBody body(&sig, decoder.pc_offset(), decoder.pc(), decoder.end());
  WasmFeatures detected;
  // We use FullValidationTag so we do not have to create another template
  // instance of WasmFullDecoder, which would cost us >50Kb binary code
  // size.
  WasmFullDecoder<Decoder::FullValidationTag, ConstantExpressionInterface,
                  kConstantExpression>
      full_decoder(zone, instance->module(), WasmFeatures::All(), &detected,
                   body, instance->module(), isolate, instance);

  full_decoder.DecodeFunctionBody();

  decoder.consume_bytes(static_cast<int>(full_decoder.pc() - decoder.pc()));

  return full_decoder.interface().has_error()
             ? ValueOrError(full_decoder.interface().error())
             : ValueOrError(full_decoder.interface().computed_value());
}

}  // namespace

base::Optional<MessageTemplate> InitializeElementSegment(
    Zone* zone, Isolate* isolate, Handle<WasmInstanceObject> instance,
    uint32_t segment_index) {
  if (!IsUndefined(instance->element_segments()->get(segment_index))) return {};

  const WasmElemSegment& elem_segment =
      instance->module()->elem_segments[segment_index];

  base::Vector<const uint8_t> module_bytes =
      instance->module_object()->native_module()->wire_bytes();

  Decoder decoder(module_bytes);
  decoder.consume_bytes(elem_segment.elements_wire_bytes_offset);

  Handle<FixedArray> result =
      isolate->factory()->NewFixedArray(elem_segment.element_count);

  for (size_t i = 0; i < elem_segment.element_count; ++i) {
    ValueOrError value =
        ConsumeElementSegmentEntry(zone, isolate, instance, elem_segment,
                                   decoder, kStrictFunctionsAndNull);
    if (is_error(value)) return {to_error(value)};
    result->set(static_cast<int>(i), *to_value(value).to_ref());
  }

  instance->element_segments()->set(segment_index, *result);

  return {};
}

void InstanceBuilder::LoadTableSegments(Handle<WasmInstanceObject> instance) {
  for (uint32_t segment_index = 0;
       segment_index < module_->elem_segments.size(); ++segment_index) {
    const WasmElemSegment& elem_segment =
        instance->module()->elem_segments[segment_index];
    // Passive segments are not copied during instantiation.
    if (elem_segment.status != WasmElemSegment::kStatusActive) continue;

    const uint32_t table_index = elem_segment.table_index;
    ValueOrError maybe_dst = EvaluateConstantExpression(
        &init_expr_zone_, elem_segment.offset, kWasmI32, isolate_, instance);
    if (MaybeMarkError(maybe_dst, thrower_)) return;
    const uint32_t dst = to_value(maybe_dst).to_u32();
    const size_t count = elem_segment.element_count;

    Handle<WasmTableObject> table_object = handle(
        WasmTableObject::cast(instance->tables()->get(table_index)), isolate_);
    if (!base::IsInBounds<size_t>(dst, count, table_object->current_length())) {
      thrower_->RuntimeError("%s",
                             MessageFormatter::TemplateString(
                                 MessageTemplate::kWasmTrapTableOutOfBounds));
      return;
    }

    base::Vector<const uint8_t> module_bytes =
        instance->module_object()->native_module()->wire_bytes();
    Decoder decoder(module_bytes);
    decoder.consume_bytes(elem_segment.elements_wire_bytes_offset);

    bool is_function_table =
        IsSubtypeOf(module_->tables[table_index].type, kWasmFuncRef, module_);

    if (is_function_table) {
      for (size_t i = 0; i < count; i++) {
        int entry_index = static_cast<int>(dst + i);
        ValueOrError computed_element = ConsumeElementSegmentEntry(
            &init_expr_zone_, isolate_, instance, elem_segment, decoder,
            kLazyFunctionsAndNull);
        if (MaybeMarkError(computed_element, thrower_)) return;

        WasmValue computed_value = to_value(computed_element);

        if (computed_value.type() == kWasmI32) {
          if (computed_value.to_i32() >= 0) {
            SetFunctionTablePlaceholder(isolate_, instance, table_object,
                                        entry_index, computed_value.to_i32());
          } else {
            SetFunctionTableNullEntry(isolate_, table_object, entry_index);
          }
        } else {
          WasmTableObject::Set(isolate_, table_object, entry_index,
                               computed_value.to_ref());
        }
      }
    } else {
      for (size_t i = 0; i < count; i++) {
        int entry_index = static_cast<int>(dst + i);
        ValueOrError computed_element = ConsumeElementSegmentEntry(
            &init_expr_zone_, isolate_, instance, elem_segment, decoder,
            kStrictFunctionsAndNull);
        if (MaybeMarkError(computed_element, thrower_)) return;
        WasmTableObject::Set(isolate_, table_object, entry_index,
                             to_value(computed_element).to_ref());
      }
    }
    // Active segment have to be set to empty after instance initialization
    // (much like passive segments after dropping).
    instance->element_segments()->set(
        segment_index, *isolate_->factory()->empty_fixed_array());
  }
}

void InstanceBuilder::InitializeTags(Handle<WasmInstanceObject> instance) {
  Handle<FixedArray> tags_table(instance->tags_table(), isolate_);
  for (int index = 0; index < tags_table->length(); ++index) {
    if (!IsUndefined(tags_table->get(index), isolate_)) continue;
    Handle<WasmExceptionTag> tag = WasmExceptionTag::New(isolate_, index);
    tags_table->set(index, *tag);
  }
}

}  // namespace v8::internal::wasm

#undef TRACE
