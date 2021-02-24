// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-wasm-support.h"

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/execution/frames-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {
namespace {

// Convert a WasmValue to an appropriate JS representation.
Handle<Object> WasmValueToObject(Isolate* isolate, wasm::WasmValue value) {
  auto* factory = isolate->factory();
  switch (value.type().kind()) {
    case wasm::ValueType::kI32:
      return factory->NewNumberFromInt(value.to_i32());
    case wasm::ValueType::kI64:
      return BigInt::FromInt64(isolate, value.to_i64());
    case wasm::ValueType::kF32:
      return factory->NewNumber(value.to_f32());
    case wasm::ValueType::kF64:
      return factory->NewNumber(value.to_f64());
    case wasm::ValueType::kS128: {
      wasm::Simd128 s128 = value.to_s128();
      Handle<JSArrayBuffer> buffer;
      if (!factory
               ->NewJSArrayBufferAndBackingStore(
                   kSimd128Size, InitializedFlag::kUninitialized)
               .ToHandle(&buffer)) {
        isolate->FatalProcessOutOfHeapMemory(
            "failed to allocate backing store");
      }

      base::Memcpy(buffer->allocation_base(), s128.bytes(),
                   buffer->byte_length());
      auto array = factory->NewJSTypedArray(kExternalUint8Array, buffer, 0,
                                            kSimd128Size);
      JSObject::SetPrototype(array, factory->null_value(), false, kDontThrow)
          .Check();
      return array;
    }
    case wasm::ValueType::kRef:
      return value.to_externref();
    default:
      break;
  }
  return factory->undefined_value();
}

}  // namespace

namespace {

// Helper for unpacking a maybe name that makes a default with an index if
// the name is empty. If the name is not empty, it's prefixed with a $.
Handle<String> GetNameOrDefault(Isolate* isolate,
                                MaybeHandle<String> maybe_name,
                                const char* default_name_prefix,
                                uint32_t index) {
  Handle<String> name;
  if (maybe_name.ToHandle(&name)) {
    name = isolate->factory()
               ->NewConsString(
                   isolate->factory()->NewStringFromAsciiChecked("$"), name)
               .ToHandleChecked();
    return isolate->factory()->InternalizeString(name);
  }
  EmbeddedVector<char, 64> value;
  int len = SNPrintF(value, "%s%u", default_name_prefix, index);
  return isolate->factory()->InternalizeString(value.SubVector(0, len));
}

enum DebugProxyId {
  kFunctionsProxy,
  kGlobalsProxy,
  kMemoriesProxy,
  kTablesProxy,
  kLastInstanceProxyId = kTablesProxy,

  kContextProxy,
  kLocalsProxy,
  kStackProxy,
  kLastProxyId = kStackProxy,

  kNumProxies = kLastProxyId + 1,
  kNumInstanceProxies = kLastInstanceProxyId + 1
};

// Creates a FixedArray with the given |length| as cache on-demand on
// the |object|, stored under the |wasm_debug_proxy_cache_symbol|.
// This is currently used to cache the debug proxy object maps on the
// JSGlobalObject (per native context), and various debug proxy objects
// (functions, globals, tables, and memories) on the WasmInstanceObject.
Handle<FixedArray> GetOrCreateDebugProxyCache(Isolate* isolate,
                                              Handle<Object> object,
                                              int length) {
  Handle<Object> cache;
  Handle<Symbol> symbol = isolate->factory()->wasm_debug_proxy_cache_symbol();
  if (!Object::GetProperty(isolate, object, symbol).ToHandle(&cache) ||
      cache->IsUndefined(isolate)) {
    cache = isolate->factory()->NewFixedArrayWithHoles(length);
    Object::SetProperty(isolate, object, symbol, cache).Check();
  } else {
    DCHECK_EQ(length, Handle<FixedArray>::cast(cache)->length());
  }
  return Handle<FixedArray>::cast(cache);
}

// Creates a Map for the given debug proxy |id| using the |create_template_fn|
// on-demand and caches this map in the global object. The map is derived from
// the FunctionTemplate returned by |create_template_fn| and has it's prototype
// set to |null| and is marked non-extensible.
Handle<Map> GetOrCreateDebugProxyMap(
    Isolate* isolate, DebugProxyId id,
    v8::Local<v8::FunctionTemplate> (*create_template_fn)(v8::Isolate*)) {
  Handle<FixedArray> maps = GetOrCreateDebugProxyCache(
      isolate, isolate->global_object(), kNumProxies);
  if (!maps->is_the_hole(isolate, id)) {
    return handle(Map::cast(maps->get(id)), isolate);
  }
  auto tmp = (*create_template_fn)(reinterpret_cast<v8::Isolate*>(isolate));
  auto fun = ApiNatives::InstantiateFunction(Utils::OpenHandle(*tmp))
                 .ToHandleChecked();
  auto map = JSFunction::GetDerivedMap(isolate, fun, fun).ToHandleChecked();
  Map::SetPrototype(isolate, map, isolate->factory()->null_value());
  map->set_is_extensible(false);
  maps->set(id, *map);
  return map;
}

// Base class for debug proxies, offers indexed access. The subclasses
// need to implement |Count| and |Get| methods appropriately.
template <typename T, DebugProxyId id, typename Provider>
struct IndexedDebugProxy {
  static constexpr DebugProxyId kId = id;

  static Handle<JSObject> Create(Isolate* isolate, Handle<Provider> provider) {
    auto object_map =
        GetOrCreateDebugProxyMap(isolate, kId, &T::CreateTemplate);
    auto object = isolate->factory()->NewJSObjectFromMap(object_map);
    object->SetEmbedderField(kProviderField, *provider);
    return object;
  }

  enum {
    kProviderField,
    kFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> CreateTemplate(v8::Isolate* isolate) {
    Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
    templ->SetClassName(
        v8::String::NewFromUtf8(isolate, T::kClassName).ToLocalChecked());
    templ->InstanceTemplate()->SetInternalFieldCount(T::kFieldCount);
    templ->InstanceTemplate()->SetHandler(
        v8::IndexedPropertyHandlerConfiguration(
            &T::IndexedGetter, {}, &T::IndexedQuery, {}, &T::IndexedEnumerator,
            {}, &T::IndexedDescriptor, {},
            v8::PropertyHandlerFlags::kHasNoSideEffect));
    return templ;
  }

  template <typename V>
  static Isolate* GetIsolate(const PropertyCallbackInfo<V>& info) {
    return reinterpret_cast<Isolate*>(info.GetIsolate());
  }

  template <typename V>
  static Handle<JSObject> GetHolder(const PropertyCallbackInfo<V>& info) {
    return Handle<JSObject>::cast(Utils::OpenHandle(*info.Holder()));
  }

  static Handle<Provider> GetProvider(Handle<JSObject> holder,
                                      Isolate* isolate) {
    return handle(Provider::cast(holder->GetEmbedderField(kProviderField)),
                  isolate);
  }

  template <typename V>
  static Handle<Provider> GetProvider(const PropertyCallbackInfo<V>& info) {
    return GetProvider(GetHolder(info), GetIsolate(info));
  }

  static void IndexedGetter(uint32_t index,
                            const PropertyCallbackInfo<v8::Value>& info) {
    auto isolate = GetIsolate(info);
    auto provider = GetProvider(info);
    if (index < T::Count(isolate, provider)) {
      auto value = T::Get(isolate, provider, index);
      info.GetReturnValue().Set(Utils::ToLocal(value));
    }
  }

  static void IndexedDescriptor(uint32_t index,
                                const PropertyCallbackInfo<v8::Value>& info) {
    auto isolate = GetIsolate(info);
    auto provider = GetProvider(info);
    if (index < T::Count(isolate, provider)) {
      PropertyDescriptor descriptor;
      descriptor.set_configurable(false);
      descriptor.set_enumerable(true);
      descriptor.set_writable(false);
      descriptor.set_value(T::Get(isolate, provider, index));
      info.GetReturnValue().Set(Utils::ToLocal(descriptor.ToObject(isolate)));
    }
  }

  static void IndexedQuery(uint32_t index,
                           const PropertyCallbackInfo<v8::Integer>& info) {
    if (index < T::Count(GetIsolate(info), GetProvider(info))) {
      info.GetReturnValue().Set(Integer::New(
          info.GetIsolate(),
          PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly));
    }
  }

  static void IndexedEnumerator(const PropertyCallbackInfo<v8::Array>& info) {
    auto isolate = GetIsolate(info);
    auto count = T::Count(isolate, GetProvider(info));
    auto indices = isolate->factory()->NewFixedArray(count);
    for (uint32_t index = 0; index < count; ++index) {
      indices->set(index, Smi::FromInt(index));
    }
    info.GetReturnValue().Set(
        Utils::ToLocal(isolate->factory()->NewJSArrayWithElements(
            indices, PACKED_SMI_ELEMENTS)));
  }
};

// Extends |IndexedDebugProxy| with named access, where the names are computed
// on-demand, and all names are assumed to start with a dollar char ($). This
// is important in order to scale to Wasm modules with hundreds of thousands
// of functions in them.
template <typename T, DebugProxyId id, typename Provider = WasmInstanceObject>
struct NamedDebugProxy : IndexedDebugProxy<T, id, Provider> {
  enum {
    kProviderField,
    kNameTableField,
    kFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> CreateTemplate(v8::Isolate* isolate) {
    auto templ = IndexedDebugProxy<T, id, Provider>::CreateTemplate(isolate);
    templ->InstanceTemplate()->SetHandler(v8::NamedPropertyHandlerConfiguration(
        &T::NamedGetter, {}, &T::NamedQuery, {}, &T::NamedEnumerator, {},
        &T::NamedDescriptor, {}, v8::PropertyHandlerFlags::kHasNoSideEffect));
    return templ;
  }

  static void IndexedEnumerator(const PropertyCallbackInfo<v8::Array>& info) {
    info.GetReturnValue().Set(v8::Array::New(info.GetIsolate()));
  }

  static Handle<NameDictionary> GetNameTable(Handle<JSObject> holder,
                                             Isolate* isolate) {
    Handle<Object> table_or_undefined(holder->GetEmbedderField(kNameTableField),
                                      isolate);
    if (!table_or_undefined->IsUndefined(isolate)) {
      return Handle<NameDictionary>::cast(table_or_undefined);
    }
    auto provider = T::GetProvider(holder, isolate);
    auto count = T::Count(isolate, provider);
    auto table = NameDictionary::New(isolate, count);
    for (uint32_t index = 0; index < count; ++index) {
      HandleScope scope(isolate);
      auto key = T::GetName(isolate, provider, index);
      if (table->FindEntry(isolate, key).is_found()) continue;
      Handle<Smi> value(Smi::FromInt(index), isolate);
      table = NameDictionary::Add(isolate, table, key, value,
                                  PropertyDetails::Empty());
    }
    holder->SetEmbedderField(kNameTableField, *table);
    return table;
  }

  template <typename V>
  static base::Optional<uint32_t> FindName(
      Local<v8::Name> name, const PropertyCallbackInfo<V>& info) {
    if (!name->IsString()) return {};
    auto name_str = Utils::OpenHandle(*name.As<v8::String>());
    if (name_str->length() == 0 || name_str->Get(0) != '$') return {};
    auto isolate = T::GetIsolate(info);
    auto table = GetNameTable(T::GetHolder(info), isolate);
    auto entry = table->FindEntry(isolate, name_str);
    if (entry.is_found()) return Smi::ToInt(table->ValueAt(entry));
    return {};
  }

  static void NamedGetter(Local<v8::Name> name,
                          const PropertyCallbackInfo<v8::Value>& info) {
    if (auto index = FindName(name, info)) T::IndexedGetter(*index, info);
  }

  static void NamedQuery(Local<v8::Name> name,
                         const PropertyCallbackInfo<v8::Integer>& info) {
    if (auto index = FindName(name, info)) T::IndexedQuery(*index, info);
  }

  static void NamedDescriptor(Local<v8::Name> name,
                              const PropertyCallbackInfo<v8::Value>& info) {
    if (auto index = FindName(name, info)) T::IndexedDescriptor(*index, info);
  }

  static void NamedEnumerator(const PropertyCallbackInfo<v8::Array>& info) {
    auto isolate = T::GetIsolate(info);
    auto table = GetNameTable(T::GetHolder(info), isolate);
    auto names = NameDictionary::IterationIndices(isolate, table);
    for (int i = 0; i < names->length(); ++i) {
      InternalIndex entry(Smi::ToInt(names->get(i)));
      names->set(i, table->NameAt(entry));
    }
    info.GetReturnValue().Set(Utils::ToLocal(
        isolate->factory()->NewJSArrayWithElements(names, PACKED_ELEMENTS)));
  }
};

// This class implements the "functions" proxy.
struct FunctionsProxy : NamedDebugProxy<FunctionsProxy, kFunctionsProxy> {
  static constexpr char const* kClassName = "Functions";

  static uint32_t Count(Isolate* isolate, Handle<WasmInstanceObject> instance) {
    return static_cast<uint32_t>(instance->module()->functions.size());
  }

  static Handle<Object> Get(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            uint32_t index) {
    return WasmInstanceObject::GetOrCreateWasmExternalFunction(isolate,
                                                               instance, index);
  }

  static Handle<String> GetName(Isolate* isolate,
                                Handle<WasmInstanceObject> instance,
                                uint32_t index) {
    wasm::ModuleWireBytes wire_bytes(
        instance->module_object().native_module()->wire_bytes());
    auto* module = instance->module();
    wasm::WireBytesRef name_ref =
        module->lazily_generated_names.LookupFunctionName(
            wire_bytes, index, VectorOf(module->export_table));
    Vector<const char> name_vec = wire_bytes.GetNameOrNull(name_ref);
    return GetNameOrDefault(
        isolate,
        name_vec.empty() ? MaybeHandle<String>()
                         : isolate->factory()->NewStringFromUtf8(name_vec),
        "$func", index);
  }
};

// This class implements the "globals" proxy.
struct GlobalsProxy : NamedDebugProxy<GlobalsProxy, kGlobalsProxy> {
  static constexpr char const* kClassName = "Globals";

  static uint32_t Count(Isolate* isolate, Handle<WasmInstanceObject> instance) {
    return static_cast<uint32_t>(instance->module()->globals.size());
  }

  static Handle<Object> Get(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            uint32_t index) {
    return WasmValueToObject(isolate,
                             WasmInstanceObject::GetGlobalValue(
                                 instance, instance->module()->globals[index]));
  }

  static Handle<String> GetName(Isolate* isolate,
                                Handle<WasmInstanceObject> instance,
                                uint32_t index) {
    return GetNameOrDefault(
        isolate,
        WasmInstanceObject::GetGlobalNameOrNull(isolate, instance, index),
        "$global", index);
  }
};

// This class implements the "memories" proxy.
struct MemoriesProxy : NamedDebugProxy<MemoriesProxy, kMemoriesProxy> {
  static constexpr char const* kClassName = "Memories";

  static uint32_t Count(Isolate* isolate, Handle<WasmInstanceObject> instance) {
    return instance->has_memory_object() ? 1 : 0;
  }

  static Handle<Object> Get(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            uint32_t index) {
    return handle(instance->memory_object(), isolate);
  }

  static Handle<String> GetName(Isolate* isolate,
                                Handle<WasmInstanceObject> instance,
                                uint32_t index) {
    return GetNameOrDefault(
        isolate,
        WasmInstanceObject::GetMemoryNameOrNull(isolate, instance, index),
        "$memory", index);
  }
};

// This class implements the "tables" proxy.
struct TablesProxy : NamedDebugProxy<TablesProxy, kTablesProxy> {
  static constexpr char const* kClassName = "Tables";

  static uint32_t Count(Isolate* isolate, Handle<WasmInstanceObject> instance) {
    return instance->tables().length();
  }

  static Handle<Object> Get(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            uint32_t index) {
    return handle(instance->tables().get(index), isolate);
  }

  static Handle<String> GetName(Isolate* isolate,
                                Handle<WasmInstanceObject> instance,
                                uint32_t index) {
    return GetNameOrDefault(
        isolate,
        WasmInstanceObject::GetTableNameOrNull(isolate, instance, index),
        "$table", index);
  }
};

// This class implements the "locals" proxy.
struct LocalsProxy : NamedDebugProxy<LocalsProxy, kLocalsProxy, FixedArray> {
  static constexpr char const* kClassName = "Locals";

  static Handle<JSObject> Create(WasmFrame* frame) {
    auto isolate = frame->isolate();
    auto debug_info = frame->native_module()->GetDebugInfo();
    // TODO(bmeurer): Check if pc is inspectable.
    int count = debug_info->GetNumLocals(frame->pc());
    auto function = debug_info->GetFunctionAtAddress(frame->pc());
    auto values = isolate->factory()->NewFixedArray(count + 2);
    for (int i = 0; i < count; ++i) {
      auto value = WasmValueToObject(
          isolate, debug_info->GetLocalValue(i, frame->pc(), frame->fp(),
                                             frame->callee_fp()));
      values->set(i, *value);
    }
    values->set(count + 0, frame->wasm_instance().module_object());
    values->set(count + 1, Smi::FromInt(function.func_index));
    return NamedDebugProxy::Create(isolate, values);
  }

  static uint32_t Count(Isolate* isolate, Handle<FixedArray> values) {
    return values->length() - 2;
  }

  static Handle<Object> Get(Isolate* isolate, Handle<FixedArray> values,
                            uint32_t index) {
    return handle(values->get(index), isolate);
  }

  static Handle<String> GetName(Isolate* isolate, Handle<FixedArray> values,
                                uint32_t index) {
    uint32_t count = Count(isolate, values);
    auto native_module =
        WasmModuleObject::cast(values->get(count + 0)).native_module();
    auto function_index = Smi::ToInt(Smi::cast(values->get(count + 1)));
    wasm::ModuleWireBytes module_wire_bytes(native_module->wire_bytes());
    auto name_vec = module_wire_bytes.GetNameOrNull(
        native_module->GetDebugInfo()->GetLocalName(function_index, index));
    return GetNameOrDefault(
        isolate,
        name_vec.empty() ? MaybeHandle<String>()
                         : isolate->factory()->NewStringFromUtf8(name_vec),
        "$var", index);
  }
};

// This class implements the "stack" proxy (which offers only indexed access).
struct StackProxy : IndexedDebugProxy<StackProxy, kStackProxy, FixedArray> {
  static constexpr char const* kClassName = "Stack";

  static Handle<JSObject> Create(WasmFrame* frame) {
    auto isolate = frame->isolate();
    auto debug_info =
        frame->wasm_instance().module_object().native_module()->GetDebugInfo();
    int count = debug_info->GetStackDepth(frame->pc());
    auto values = isolate->factory()->NewFixedArray(count);
    for (int i = 0; i < count; ++i) {
      auto value = WasmValueToObject(
          isolate, debug_info->GetStackValue(i, frame->pc(), frame->fp(),
                                             frame->callee_fp()));
      values->set(i, *value);
    }
    return IndexedDebugProxy::Create(isolate, values);
  }

  static uint32_t Count(Isolate* isolate, Handle<FixedArray> values) {
    return values->length();
  }

  static Handle<Object> Get(Isolate* isolate, Handle<FixedArray> values,
                            uint32_t index) {
    return handle(values->get(index), isolate);
  }
};

// Creates an instance of the |Proxy| on-demand and caches that on the
// |instance|.
template <typename Proxy>
Handle<JSObject> GetOrCreateInstanceProxy(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance) {
  STATIC_ASSERT(Proxy::kId < kNumInstanceProxies);
  Handle<FixedArray> proxies =
      GetOrCreateDebugProxyCache(isolate, instance, kNumInstanceProxies);
  if (!proxies->is_the_hole(isolate, Proxy::kId)) {
    return handle(JSObject::cast(proxies->get(Proxy::kId)), isolate);
  }
  Handle<JSObject> proxy = Proxy::Create(isolate, instance);
  proxies->set(Proxy::kId, *proxy);
  return proxy;
}

// This class implements the debug proxy for a given Wasm frame. The debug
// proxy is used when evaluating JavaScript expressions on a wasm frame via
// the inspector |Runtime.evaluateOnCallFrame()| API and enables developers
// and extensions to inspect the WebAssembly engine state from JavaScript.
// The proxy provides the following interface:
//
// type WasmSimdValue = Uint8Array;
// type WasmValue = number | bigint | object | WasmSimdValue;
// type WasmFunction = (... args : WasmValue[]) = > WasmValue;
// interface WasmInterface {
//   $globalX: WasmValue;
//   $varX: WasmValue;
//   $funcX(a : WasmValue /*, ...*/) : WasmValue;
//   readonly $memoryX : WebAssembly.Memory;
//   readonly $tableX : WebAssembly.Table;
//
//   readonly instance : WebAssembly.Instance;
//   readonly module : WebAssembly.Module;
//
//   readonly memories : {[nameOrIndex:string | number] : WebAssembly.Memory};
//   readonly tables : {[nameOrIndex:string | number] : WebAssembly.Table};
//   readonly stack : WasmValue[];
//   readonly globals : {[nameOrIndex:string | number] : WasmValue};
//   readonly locals : {[nameOrIndex:string | number] : WasmValue};
//   readonly functions : {[nameOrIndex:string | number] : WasmFunction};
// }
//
// The wasm index spaces memories, tables, stack, globals, locals, and
// functions are JSObjects with interceptors that lazily produce values
// either by index or by name (except for stack).
// Only the names are reported by APIs such as Object.keys() and
// Object.getOwnPropertyNames(), since the indices are not meant to be
// used interactively by developers (in Chrome DevTools), but are provided
// for WebAssembly language extensions. Also note that these JSObjects
// all have null prototypes, to not confuse context lookup and to make
// their purpose as dictionaries clear.
//
// See http://doc/1VZOJrU2VsqOZe3IUzbwQWQQSZwgGySsm5119Ust1gUA and
// http://bit.ly/devtools-wasm-entities for more details.
class ContextProxy {
 public:
  static Handle<JSObject> Create(WasmFrame* frame) {
    Isolate* isolate = frame->isolate();
    auto object_map =
        GetOrCreateDebugProxyMap(isolate, kContextProxy, &CreateTemplate);
    auto object = isolate->factory()->NewJSObjectFromMap(object_map);
    Handle<WasmInstanceObject> instance(frame->wasm_instance(), isolate);
    object->SetEmbedderField(kInstanceField, *instance);
    Handle<JSObject> locals = LocalsProxy::Create(frame);
    object->SetEmbedderField(kLocalsField, *locals);
    Handle<JSObject> stack = StackProxy::Create(frame);
    object->SetEmbedderField(kStackField, *stack);
    return object;
  }

 private:
  enum {
    kInstanceField,
    kLocalsField,
    kStackField,
    kFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> CreateTemplate(v8::Isolate* isolate) {
    Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
    templ->InstanceTemplate()->SetInternalFieldCount(kFieldCount);
    templ->InstanceTemplate()->SetHandler(v8::NamedPropertyHandlerConfiguration(
        &NamedGetter, {}, {}, {}, {}, {}, {}, {},
        static_cast<v8::PropertyHandlerFlags>(
            static_cast<unsigned>(
                v8::PropertyHandlerFlags::kOnlyInterceptStrings) |
            static_cast<unsigned>(
                v8::PropertyHandlerFlags::kHasNoSideEffect))));
    return templ;
  }

  static MaybeHandle<Object> GetNamedProperty(Isolate* isolate,
                                              Handle<JSObject> holder,
                                              Handle<String> name) {
    if (name->length() == 0) return {};
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(holder->GetEmbedderField(kInstanceField)),
        isolate);
    if (name->IsOneByteEqualTo(StaticCharVector("instance"))) {
      return instance;
    }
    if (name->IsOneByteEqualTo(StaticCharVector("module"))) {
      return handle(instance->module_object(), isolate);
    }
    if (name->IsOneByteEqualTo(StaticCharVector("locals"))) {
      return handle(holder->GetEmbedderField(kLocalsField), isolate);
    }
    if (name->IsOneByteEqualTo(StaticCharVector("stack"))) {
      return handle(holder->GetEmbedderField(kStackField), isolate);
    }
    if (name->IsOneByteEqualTo(StaticCharVector("memories"))) {
      return GetOrCreateInstanceProxy<MemoriesProxy>(isolate, instance);
    }
    if (name->IsOneByteEqualTo(StaticCharVector("tables"))) {
      return GetOrCreateInstanceProxy<TablesProxy>(isolate, instance);
    }
    if (name->IsOneByteEqualTo(StaticCharVector("globals"))) {
      return GetOrCreateInstanceProxy<GlobalsProxy>(isolate, instance);
    }
    if (name->IsOneByteEqualTo(StaticCharVector("functions"))) {
      return GetOrCreateInstanceProxy<FunctionsProxy>(isolate, instance);
    }
    if (name->Get(0) == '$') {
      const char* kDelegateNames[] = {"memories", "locals", "tables",
                                      "functions", "globals"};
      for (auto delegate_name : kDelegateNames) {
        Handle<Object> delegate;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, delegate,
            JSObject::GetProperty(isolate, holder, delegate_name), Object);
        if (!delegate->IsUndefined(isolate)) {
          Handle<Object> value;
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value, Object::GetProperty(isolate, delegate, name),
              Object);
          if (!value->IsUndefined(isolate)) return value;
        }
      }
    }
    return {};
  }

  static void NamedGetter(Local<v8::Name> name,
                          const PropertyCallbackInfo<v8::Value>& info) {
    auto name_string = Handle<String>::cast(Utils::OpenHandle(*name));
    auto isolate = reinterpret_cast<Isolate*>(info.GetIsolate());
    auto holder = Handle<JSObject>::cast(Utils::OpenHandle(*info.Holder()));
    Handle<Object> value;
    if (GetNamedProperty(isolate, holder, name_string).ToHandle(&value)) {
      info.GetReturnValue().Set(Utils::ToLocal(value));
    }
  }
};

Handle<JSObject> GetModuleScopeObject(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  Handle<JSObject> module_scope_object =
      isolate->factory()->NewJSObjectWithNullProto();

  Handle<String> instance_name =
      isolate->factory()->InternalizeString(StaticCharVector("instance"));
  JSObject::AddProperty(isolate, module_scope_object, instance_name, instance,
                        NONE);

  Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
  Handle<String> module_name =
      isolate->factory()->InternalizeString(StaticCharVector("module"));
  JSObject::AddProperty(isolate, module_scope_object, module_name,
                        module_object, NONE);

  uint32_t memory_count = MemoriesProxy::Count(isolate, instance);
  for (uint32_t memory_index = 0; memory_index < memory_count; ++memory_index) {
    auto memory_name = MemoriesProxy::GetName(isolate, instance, memory_index);
    auto memory_value = MemoriesProxy::Get(isolate, instance, memory_index);
    JSObject::AddProperty(isolate, module_scope_object, memory_name,
                          memory_value, NONE);
  }

  if (GlobalsProxy::Count(isolate, instance) != 0) {
    Handle<JSObject> globals_obj =
        GetOrCreateInstanceProxy<GlobalsProxy>(isolate, instance);
    Handle<String> globals_name =
        isolate->factory()->InternalizeString(StaticCharVector("globals"));
    JSObject::AddProperty(isolate, module_scope_object, globals_name,
                          globals_obj, NONE);
  }

  return module_scope_object;
}

class DebugWasmScopeIterator final : public debug::ScopeIterator {
 public:
  explicit DebugWasmScopeIterator(WasmFrame* frame)
      : frame_(frame),
        type_(debug::ScopeIterator::ScopeTypeWasmExpressionStack) {
    // Skip local scope and expression stack scope if the frame is not
    // inspectable.
    if (!frame->is_inspectable()) {
      type_ = debug::ScopeIterator::ScopeTypeModule;
    }
  }

  bool Done() override { return type_ == ScopeTypeWith; }

  void Advance() override {
    DCHECK(!Done());
    switch (type_) {
      case ScopeTypeWasmExpressionStack:
        type_ = debug::ScopeIterator::ScopeTypeLocal;
        break;
      case ScopeTypeLocal:
        type_ = debug::ScopeIterator::ScopeTypeModule;
        break;
      case ScopeTypeModule:
        // We use ScopeTypeWith type as marker for done.
        type_ = debug::ScopeIterator::ScopeTypeWith;
        break;
      default:
        UNREACHABLE();
    }
  }

  ScopeType GetType() override { return type_; }

  v8::Local<v8::Object> GetObject() override {
    DCHECK(!Done());
    switch (type_) {
      case debug::ScopeIterator::ScopeTypeModule: {
        Handle<WasmInstanceObject> instance =
            FrameSummary::GetTop(frame_).AsWasm().wasm_instance();
        return Utils::ToLocal(GetModuleScopeObject(instance));
      }
      case debug::ScopeIterator::ScopeTypeLocal: {
        DCHECK(frame_->is_inspectable());
        return Utils::ToLocal(LocalsProxy::Create(frame_));
      }
      case debug::ScopeIterator::ScopeTypeWasmExpressionStack: {
        DCHECK(frame_->is_inspectable());
        return Utils::ToLocal(StackProxy::Create(frame_));
      }
      default:
        return {};
    }
  }
  v8::Local<v8::Value> GetFunctionDebugName() override {
    return Utils::ToLocal(frame_->isolate()->factory()->empty_string());
  }

  int GetScriptId() override { return -1; }

  bool HasLocationInfo() override { return false; }

  debug::Location GetStartLocation() override { return {}; }

  debug::Location GetEndLocation() override { return {}; }

  bool SetVariableValue(v8::Local<v8::String> name,
                        v8::Local<v8::Value> value) override {
    return false;
  }

 private:
  WasmFrame* const frame_;
  ScopeType type_;
};

}  // namespace

Handle<JSObject> GetWasmDebugProxy(WasmFrame* frame) {
  return ContextProxy::Create(frame);
}

std::unique_ptr<debug::ScopeIterator> GetWasmScopeIterator(WasmFrame* frame) {
  return std::make_unique<DebugWasmScopeIterator>(frame);
}

}  // namespace internal
}  // namespace v8
