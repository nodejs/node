// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-wasm-objects.h"

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/debug/debug-wasm-objects-inl.h"
#include "src/execution/frames-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {
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

MaybeHandle<String> GetNameFromImportsAndExportsOrNull(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    wasm::ImportExportKindCode kind, uint32_t index) {
  auto debug_info = instance->module_object().native_module()->GetDebugInfo();
  wasm::ModuleWireBytes wire_bytes(
      instance->module_object().native_module()->wire_bytes());

  auto import_name_ref = debug_info->GetImportName(kind, index);
  if (!import_name_ref.first.is_empty()) {
    ScopedVector<char> name(import_name_ref.first.length() + 1 +
                            import_name_ref.second.length());
    auto name_begin = &name.first(), name_end = name_begin;
    auto module_name = wire_bytes.GetNameOrNull(import_name_ref.first);
    name_end = std::copy(module_name.begin(), module_name.end(), name_end);
    *name_end++ = '.';
    auto field_name = wire_bytes.GetNameOrNull(import_name_ref.second);
    name_end = std::copy(field_name.begin(), field_name.end(), name_end);
    return isolate->factory()->NewStringFromUtf8(
        VectorOf(name_begin, name_end - name_begin));
  }

  auto export_name_ref = debug_info->GetExportName(kind, index);
  if (!export_name_ref.is_empty()) {
    auto name = wire_bytes.GetNameOrNull(export_name_ref);
    return isolate->factory()->NewStringFromUtf8(name);
  }

  return {};
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
  kStructProxy,
  kArrayProxy,
  kLastProxyId = kArrayProxy,

  kNumProxies = kLastProxyId + 1,
  kNumInstanceProxies = kLastInstanceProxyId + 1
};

constexpr int kWasmValueMapIndex = kNumProxies;
constexpr int kNumDebugMaps = kWasmValueMapIndex + 1;

Handle<FixedArray> GetOrCreateDebugMaps(Isolate* isolate) {
  Handle<FixedArray> maps = isolate->wasm_debug_maps();
  if (maps->length() == 0) {
    maps = isolate->factory()->NewFixedArrayWithHoles(kNumDebugMaps);
    isolate->native_context()->set_wasm_debug_maps(*maps);
  }
  return maps;
}

// Creates a Map for the given debug proxy |id| using the |create_template_fn|
// on-demand and caches this map in the global object. The map is derived from
// the FunctionTemplate returned by |create_template_fn| and has its prototype
// set to |null| and is marked non-extensible (by default).
// TODO(bmeurer): remove the extensibility opt-out and replace it with a proper
// way to add non-intercepted named properties.
Handle<Map> GetOrCreateDebugProxyMap(
    Isolate* isolate, DebugProxyId id,
    v8::Local<v8::FunctionTemplate> (*create_template_fn)(v8::Isolate*),
    bool make_non_extensible = true) {
  auto maps = GetOrCreateDebugMaps(isolate);
  CHECK_LE(kNumProxies, maps->length());
  if (!maps->is_the_hole(isolate, id)) {
    return handle(Map::cast(maps->get(id)), isolate);
  }
  auto tmp = (*create_template_fn)(reinterpret_cast<v8::Isolate*>(isolate));
  auto fun = ApiNatives::InstantiateFunction(Utils::OpenHandle(*tmp))
                 .ToHandleChecked();
  auto map = JSFunction::GetDerivedMap(isolate, fun, fun).ToHandleChecked();
  Map::SetPrototype(isolate, map, isolate->factory()->null_value());
  if (make_non_extensible) {
    map->set_is_extensible(false);
  }
  maps->set(id, *map);
  return map;
}

// Base class for debug proxies, offers indexed access. The subclasses
// need to implement |Count| and |Get| methods appropriately.
template <typename T, DebugProxyId id, typename Provider>
struct IndexedDebugProxy {
  static constexpr DebugProxyId kId = id;

  static Handle<JSObject> Create(Isolate* isolate, Handle<Provider> provider,
                                 bool make_map_non_extensible = true) {
    auto object_map = GetOrCreateDebugProxyMap(isolate, kId, &T::CreateTemplate,
                                               make_map_non_extensible);
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
    Handle<Symbol> symbol = isolate->factory()->wasm_debug_proxy_names_symbol();
    Handle<Object> table_or_undefined =
        JSObject::GetProperty(isolate, holder, symbol).ToHandleChecked();
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
    Object::SetProperty(isolate, holder, symbol, table).Check();
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
    Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
    MaybeHandle<String> name =
        WasmModuleObject::GetFunctionNameOrNull(isolate, module_object, index);
    if (name.is_null()) {
      name = GetNameFromImportsAndExportsOrNull(
          isolate, instance, wasm::ImportExportKindCode::kExternalFunction,
          index);
    }
    return GetNameOrDefault(isolate, name, "$func", index);
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
    Handle<WasmModuleObject> module(instance->module_object(), isolate);
    return WasmValueObject::New(
        isolate,
        WasmInstanceObject::GetGlobalValue(instance,
                                           instance->module()->globals[index]),
        module);
  }

  static Handle<String> GetName(Isolate* isolate,
                                Handle<WasmInstanceObject> instance,
                                uint32_t index) {
    return GetNameOrDefault(
        isolate,
        GetNameFromImportsAndExportsOrNull(
            isolate, instance, wasm::ImportExportKindCode::kExternalGlobal,
            index),
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
        GetNameFromImportsAndExportsOrNull(
            isolate, instance, wasm::ImportExportKindCode::kExternalMemory,
            index),
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
        GetNameFromImportsAndExportsOrNull(
            isolate, instance, wasm::ImportExportKindCode::kExternalTable,
            index),
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
    Handle<WasmModuleObject> module_object(
        frame->wasm_instance().module_object(), isolate);
    for (int i = 0; i < count; ++i) {
      auto value = WasmValueObject::New(
          isolate,
          debug_info->GetLocalValue(i, frame->pc(), frame->fp(),
                                    frame->callee_fp(), isolate),
          module_object);
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
    Handle<WasmModuleObject> module_object(
        frame->wasm_instance().module_object(), isolate);
    for (int i = 0; i < count; ++i) {
      auto value = WasmValueObject::New(
          isolate,
          debug_info->GetStackValue(i, frame->pc(), frame->fp(),
                                    frame->callee_fp(), isolate),
          module_object);
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

// Creates FixedArray with size |kNumInstanceProxies| as cache on-demand
// on the |instance|, stored under the |wasm_debug_proxy_cache_symbol|.
// This is used to cache the various instance debug proxies (functions,
// globals, tables, and memories) on the WasmInstanceObject.
Handle<FixedArray> GetOrCreateInstanceProxyCache(
    Isolate* isolate, Handle<WasmInstanceObject> instance) {
  Handle<Object> cache;
  Handle<Symbol> symbol = isolate->factory()->wasm_debug_proxy_cache_symbol();
  if (!Object::GetProperty(isolate, instance, symbol).ToHandle(&cache) ||
      cache->IsUndefined(isolate)) {
    cache = isolate->factory()->NewFixedArrayWithHoles(kNumInstanceProxies);
    Object::SetProperty(isolate, instance, symbol, cache).Check();
  }
  return Handle<FixedArray>::cast(cache);
}

// Creates an instance of the |Proxy| on-demand and caches that on the
// |instance|.
template <typename Proxy>
Handle<JSObject> GetOrCreateInstanceProxy(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance) {
  STATIC_ASSERT(Proxy::kId < kNumInstanceProxies);
  Handle<FixedArray> proxies = GetOrCreateInstanceProxyCache(isolate, instance);
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
// type WasmValue = {
//   type: string;
//   value: number | bigint | object | string;
// };
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
class ContextProxyPrototype {
 public:
  static Handle<JSObject> Create(Isolate* isolate) {
    auto object_map =
        GetOrCreateDebugProxyMap(isolate, kContextProxy, &CreateTemplate);
    return isolate->factory()->NewJSObjectFromMap(object_map);
  }

 private:
  static v8::Local<v8::FunctionTemplate> CreateTemplate(v8::Isolate* isolate) {
    Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
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
                                              Handle<JSObject> receiver,
                                              Handle<String> name) {
    if (name->length() != 0 && name->Get(0) == '$') {
      const char* kDelegateNames[] = {"memories", "locals", "tables",
                                      "functions", "globals"};
      for (auto delegate_name : kDelegateNames) {
        Handle<Object> delegate;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, delegate,
            JSObject::GetProperty(isolate, receiver, delegate_name), Object);
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
    auto receiver = Handle<JSObject>::cast(Utils::OpenHandle(*info.This()));
    Handle<Object> value;
    if (GetNamedProperty(isolate, receiver, name_string).ToHandle(&value)) {
      info.GetReturnValue().Set(Utils::ToLocal(value));
    }
  }
};

class ContextProxy {
 public:
  static Handle<JSObject> Create(WasmFrame* frame) {
    Isolate* isolate = frame->isolate();
    auto object = isolate->factory()->NewJSObjectWithNullProto();
    Handle<WasmInstanceObject> instance(frame->wasm_instance(), isolate);
    JSObject::AddProperty(isolate, object, "instance", instance, FROZEN);
    Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
    JSObject::AddProperty(isolate, object, "module", module_object, FROZEN);
    auto locals = LocalsProxy::Create(frame);
    JSObject::AddProperty(isolate, object, "locals", locals, FROZEN);
    auto stack = StackProxy::Create(frame);
    JSObject::AddProperty(isolate, object, "stack", stack, FROZEN);
    auto memories = GetOrCreateInstanceProxy<MemoriesProxy>(isolate, instance);
    JSObject::AddProperty(isolate, object, "memories", memories, FROZEN);
    auto tables = GetOrCreateInstanceProxy<TablesProxy>(isolate, instance);
    JSObject::AddProperty(isolate, object, "tables", tables, FROZEN);
    auto globals = GetOrCreateInstanceProxy<GlobalsProxy>(isolate, instance);
    JSObject::AddProperty(isolate, object, "globals", globals, FROZEN);
    auto functions =
        GetOrCreateInstanceProxy<FunctionsProxy>(isolate, instance);
    JSObject::AddProperty(isolate, object, "functions", functions, FROZEN);
    Handle<JSObject> prototype = ContextProxyPrototype::Create(isolate);
    JSObject::SetPrototype(object, prototype, false, kDontThrow).Check();
    return object;
  }
};

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
    Isolate* isolate = frame_->isolate();
    switch (type_) {
      case debug::ScopeIterator::ScopeTypeModule: {
        Handle<WasmInstanceObject> instance(frame_->wasm_instance(), isolate);
        Handle<JSObject> object =
            isolate->factory()->NewJSObjectWithNullProto();
        JSObject::AddProperty(isolate, object, "instance", instance, FROZEN);
        Handle<JSObject> module_object(instance->module_object(), isolate);
        JSObject::AddProperty(isolate, object, "module", module_object, FROZEN);
        if (FunctionsProxy::Count(isolate, instance) != 0) {
          JSObject::AddProperty(
              isolate, object, "functions",
              GetOrCreateInstanceProxy<FunctionsProxy>(isolate, instance),
              FROZEN);
        }
        if (GlobalsProxy::Count(isolate, instance) != 0) {
          JSObject::AddProperty(
              isolate, object, "globals",
              GetOrCreateInstanceProxy<GlobalsProxy>(isolate, instance),
              FROZEN);
        }
        if (MemoriesProxy::Count(isolate, instance) != 0) {
          JSObject::AddProperty(
              isolate, object, "memories",
              GetOrCreateInstanceProxy<MemoriesProxy>(isolate, instance),
              FROZEN);
        }
        if (TablesProxy::Count(isolate, instance) != 0) {
          JSObject::AddProperty(
              isolate, object, "tables",
              GetOrCreateInstanceProxy<TablesProxy>(isolate, instance), FROZEN);
        }
        return Utils::ToLocal(object);
      }
      case debug::ScopeIterator::ScopeTypeLocal: {
        return Utils::ToLocal(LocalsProxy::Create(frame_));
      }
      case debug::ScopeIterator::ScopeTypeWasmExpressionStack: {
        auto object = isolate->factory()->NewJSObjectWithNullProto();
        auto stack = StackProxy::Create(frame_);
        JSObject::AddProperty(isolate, object, "stack", stack, FROZEN);
        return Utils::ToLocal(object);
      }
      default:
        UNREACHABLE();
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

Handle<String> WasmSimd128ToString(Isolate* isolate, wasm::Simd128 s128) {
  // We use the canonical format as described in:
  // https://github.com/WebAssembly/simd/blob/master/proposals/simd/TextSIMD.md
  EmbeddedVector<char, 50> buffer;
  auto i32x4 = s128.to_i32x4();
  SNPrintF(buffer, "i32x4 0x%08X 0x%08X 0x%08X 0x%08X", i32x4.val[0],
           i32x4.val[1], i32x4.val[2], i32x4.val[3]);
  return isolate->factory()->NewStringFromAsciiChecked(buffer.data());
}

Handle<String> GetRefTypeName(Isolate* isolate, wasm::ValueType type,
                              wasm::NativeModule* module) {
  const char* nullable = type.kind() == wasm::kOptRef ? " null" : "";
  EmbeddedVector<char, 64> type_name;
  size_t len;
  if (type.heap_type().is_generic()) {
    const char* generic_name = "";
    wasm::HeapType::Representation heap_rep = type.heap_representation();
    switch (heap_rep) {
      case wasm::HeapType::kFunc:
        generic_name = "func";
        break;
      case wasm::HeapType::kExtern:
        generic_name = "extern";
        break;
      case wasm::HeapType::kEq:
        generic_name = "eq";
        break;
      case wasm::HeapType::kI31:
        generic_name = "i31";
        break;
      case wasm::HeapType::kData:
        generic_name = "data";
        break;
      case wasm::HeapType::kAny:
        generic_name = "any";
        break;
      default:
        UNREACHABLE();
    }
    len = SNPrintF(type_name, "(ref%s %s)", nullable, generic_name);
  } else {
    int type_index = type.ref_index();
    wasm::ModuleWireBytes module_wire_bytes(module->wire_bytes());
    Vector<const char> name_vec = module_wire_bytes.GetNameOrNull(
        module->GetDebugInfo()->GetTypeName(type_index));
    if (name_vec.empty()) {
      len = SNPrintF(type_name, "(ref%s $type%u)", nullable, type_index);
    } else {
      len = SNPrintF(type_name, "(ref%s $", nullable);
      Vector<char> suffix = type_name.SubVector(len, type_name.size());
      StrNCpy(suffix, name_vec.data(), name_vec.size());
      len += std::min(suffix.size(), name_vec.size());
      if (len < type_name.size()) {
        type_name[len] = ')';
        len++;
      }
    }
  }
  return isolate->factory()->InternalizeString(type_name.SubVector(0, len));
}

}  // namespace

// static
Handle<WasmValueObject> WasmValueObject::New(Isolate* isolate,
                                             Handle<String> type,
                                             Handle<Object> value) {
  auto maps = GetOrCreateDebugMaps(isolate);
  if (maps->is_the_hole(isolate, kWasmValueMapIndex)) {
    Handle<Map> map = isolate->factory()->NewMap(
        WASM_VALUE_OBJECT_TYPE, WasmValueObject::kSize,
        TERMINAL_FAST_ELEMENTS_KIND, 2);
    Map::EnsureDescriptorSlack(isolate, map, 2);
    {  // type
      Descriptor d = Descriptor::DataField(
          isolate,
          isolate->factory()->InternalizeString(StaticCharVector("type")),
          WasmValueObject::kTypeIndex, FROZEN, Representation::Tagged());
      map->AppendDescriptor(isolate, &d);
    }
    {  // value
      Descriptor d = Descriptor::DataField(
          isolate,
          isolate->factory()->InternalizeString(StaticCharVector("value")),
          WasmValueObject::kValueIndex, FROZEN, Representation::Tagged());
      map->AppendDescriptor(isolate, &d);
    }
    map->set_is_extensible(false);
    maps->set(kWasmValueMapIndex, *map);
  }
  Handle<Map> value_map =
      handle(Map::cast(maps->get(kWasmValueMapIndex)), isolate);
  Handle<WasmValueObject> object = Handle<WasmValueObject>::cast(
      isolate->factory()->NewJSObjectFromMap(value_map));
  object->set_type(*type);
  object->set_value(*value);
  return object;
}

// This class implements a proxy for a single inspectable Wasm struct.
struct StructProxy : NamedDebugProxy<StructProxy, kStructProxy, FixedArray> {
  static constexpr char const* kClassName = "Struct";

  static const int kObjectIndex = 0;
  static const int kModuleIndex = 1;
  static const int kTypeIndexIndex = 2;
  static const int kLength = 3;

  static Handle<JSObject> Create(Isolate* isolate, const wasm::WasmValue& value,
                                 Handle<WasmModuleObject> module) {
    Handle<FixedArray> data = isolate->factory()->NewFixedArray(kLength);
    data->set(kObjectIndex, *value.to_ref());
    data->set(kModuleIndex, *module);
    int struct_type_index = value.type().ref_index();
    data->set(kTypeIndexIndex, Smi::FromInt(struct_type_index));
    return NamedDebugProxy::Create(isolate, data);
  }

  static uint32_t Count(Isolate* isolate, Handle<FixedArray> data) {
    return WasmStruct::cast(data->get(kObjectIndex)).type()->field_count();
  }

  static Handle<Object> Get(Isolate* isolate, Handle<FixedArray> data,
                            uint32_t index) {
    Handle<WasmStruct> obj(WasmStruct::cast(data->get(kObjectIndex)), isolate);
    Handle<WasmModuleObject> module(
        WasmModuleObject::cast(data->get(kModuleIndex)), isolate);
    return WasmValueObject::New(isolate, obj->GetFieldValue(index), module);
  }

  static Handle<String> GetName(Isolate* isolate, Handle<FixedArray> data,
                                uint32_t index) {
    wasm::NativeModule* native_module =
        WasmModuleObject::cast(data->get(kModuleIndex)).native_module();
    int struct_type_index = Smi::ToInt(Smi::cast(data->get(kTypeIndexIndex)));
    wasm::ModuleWireBytes module_wire_bytes(native_module->wire_bytes());
    Vector<const char> name_vec = module_wire_bytes.GetNameOrNull(
        native_module->GetDebugInfo()->GetFieldName(struct_type_index, index));
    return GetNameOrDefault(
        isolate,
        name_vec.empty() ? MaybeHandle<String>()
                         : isolate->factory()->NewStringFromUtf8(name_vec),
        "$field", index);
  }
};

// This class implements a proxy for a single inspectable Wasm array.
struct ArrayProxy : IndexedDebugProxy<ArrayProxy, kArrayProxy, FixedArray> {
  static constexpr char const* kClassName = "Array";

  static const int kObjectIndex = 0;
  static const int kModuleIndex = 1;
  static const int kLength = 2;

  static Handle<JSObject> Create(Isolate* isolate, const wasm::WasmValue& value,
                                 Handle<WasmModuleObject> module) {
    Handle<FixedArray> data = isolate->factory()->NewFixedArray(kLength);
    data->set(kObjectIndex, *value.to_ref());
    data->set(kModuleIndex, *module);
    Handle<JSObject> proxy = IndexedDebugProxy::Create(
        isolate, data, false /* leave map extensible */);
    uint32_t length = WasmArray::cast(*value.to_ref()).length();
    Handle<Object> length_obj = isolate->factory()->NewNumberFromUint(length);
    Object::SetProperty(isolate, proxy, isolate->factory()->length_string(),
                        length_obj, StoreOrigin::kNamed,
                        Just(ShouldThrow::kThrowOnError))
        .Check();
    return proxy;
  }

  static v8::Local<v8::FunctionTemplate> CreateTemplate(v8::Isolate* isolate) {
    Local<v8::FunctionTemplate> templ =
        IndexedDebugProxy::CreateTemplate(isolate);
    templ->InstanceTemplate()->Set(isolate, "length",
                                   v8::Number::New(isolate, 0));
    return templ;
  }

  static uint32_t Count(Isolate* isolate, Handle<FixedArray> data) {
    return WasmArray::cast(data->get(kObjectIndex)).length();
  }

  static Handle<Object> Get(Isolate* isolate, Handle<FixedArray> data,
                            uint32_t index) {
    Handle<WasmArray> array(WasmArray::cast(data->get(kObjectIndex)), isolate);
    Handle<WasmModuleObject> module(
        WasmModuleObject::cast(data->get(kModuleIndex)), isolate);
    return WasmValueObject::New(isolate, array->GetElement(index), module);
  }
};

// static
Handle<WasmValueObject> WasmValueObject::New(
    Isolate* isolate, const wasm::WasmValue& value,
    Handle<WasmModuleObject> module_object) {
  Handle<String> t;
  Handle<Object> v;
  switch (value.type().kind()) {
    case wasm::kI8: {
      // This can't be reached for most "top-level" things, only via nested
      // calls for struct/array fields.
      t = isolate->factory()->InternalizeString(StaticCharVector("i8"));
      v = isolate->factory()->NewNumber(value.to_i8_unchecked());
      break;
    }
    case wasm::kI16: {
      // This can't be reached for most "top-level" things, only via nested
      // calls for struct/array fields.
      t = isolate->factory()->InternalizeString(StaticCharVector("i16"));
      v = isolate->factory()->NewNumber(value.to_i16_unchecked());
      break;
    }
    case wasm::kI32: {
      t = isolate->factory()->InternalizeString(StaticCharVector("i32"));
      v = isolate->factory()->NewNumberFromInt(value.to_i32_unchecked());
      break;
    }
    case wasm::kI64: {
      t = isolate->factory()->InternalizeString(StaticCharVector("i64"));
      v = BigInt::FromInt64(isolate, value.to_i64_unchecked());
      break;
    }
    case wasm::kF32: {
      t = isolate->factory()->InternalizeString(StaticCharVector("f32"));
      v = isolate->factory()->NewNumber(value.to_f32_unchecked());
      break;
    }
    case wasm::kF64: {
      t = isolate->factory()->InternalizeString(StaticCharVector("f64"));
      v = isolate->factory()->NewNumber(value.to_f64_unchecked());
      break;
    }
    case wasm::kS128: {
      t = isolate->factory()->InternalizeString(StaticCharVector("v128"));
      v = WasmSimd128ToString(isolate, value.to_s128_unchecked());
      break;
    }
    case wasm::kOptRef:
      if (value.type().is_reference_to(wasm::HeapType::kExtern)) {
        t = isolate->factory()->InternalizeString(
            StaticCharVector("externref"));
        v = value.to_ref();
        break;
      }
      V8_FALLTHROUGH;
    case wasm::kRef: {
      t = GetRefTypeName(isolate, value.type(), module_object->native_module());
      Handle<Object> ref = value.to_ref();
      if (ref->IsWasmStruct()) {
        v = StructProxy::Create(isolate, value, module_object);
      } else if (ref->IsWasmArray()) {
        v = ArrayProxy::Create(isolate, value, module_object);
      } else if (ref->IsJSFunction() || ref->IsSmi() || ref->IsNull()) {
        v = ref;
      } else {
        // Fail gracefully.
        EmbeddedVector<char, 64> error;
        int len = SNPrintF(error, "unimplemented object type: %d",
                           HeapObject::cast(*ref).map().instance_type());
        v = isolate->factory()->InternalizeString(error.SubVector(0, len));
      }
      break;
    }
    case wasm::kRtt:
    case wasm::kRttWithDepth: {
      // TODO(7748): Expose RTTs to DevTools.
      t = isolate->factory()->InternalizeString(StaticCharVector("rtt"));
      v = isolate->factory()->InternalizeString(
          StaticCharVector("(unimplemented)"));
      break;
    }
    case wasm::kVoid:
    case wasm::kBottom:
      UNREACHABLE();
  }
  return New(isolate, t, v);
}

Handle<JSObject> GetWasmDebugProxy(WasmFrame* frame) {
  return ContextProxy::Create(frame);
}

std::unique_ptr<debug::ScopeIterator> GetWasmScopeIterator(WasmFrame* frame) {
  return std::make_unique<DebugWasmScopeIterator>(frame);
}

Handle<JSArray> GetWasmInstanceObjectInternalProperties(
    Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  Handle<FixedArray> result = isolate->factory()->NewFixedArray(2 * 5);
  int length = 0;

  Handle<String> module_str =
      isolate->factory()->NewStringFromAsciiChecked("[[Module]]");
  Handle<Object> module_obj = handle(instance->module_object(), isolate);
  result->set(length++, *module_str);
  result->set(length++, *module_obj);

  if (FunctionsProxy::Count(isolate, instance) != 0) {
    Handle<String> functions_str =
        isolate->factory()->NewStringFromAsciiChecked("[[Functions]]");
    Handle<Object> functions_obj =
        GetOrCreateInstanceProxy<FunctionsProxy>(isolate, instance);
    result->set(length++, *functions_str);
    result->set(length++, *functions_obj);
  }

  if (GlobalsProxy::Count(isolate, instance) != 0) {
    Handle<String> globals_str =
        isolate->factory()->NewStringFromAsciiChecked("[[Globals]]");
    Handle<Object> globals_obj =
        GetOrCreateInstanceProxy<GlobalsProxy>(isolate, instance);
    result->set(length++, *globals_str);
    result->set(length++, *globals_obj);
  }

  if (MemoriesProxy::Count(isolate, instance) != 0) {
    Handle<String> memories_str =
        isolate->factory()->NewStringFromAsciiChecked("[[Memories]]");
    Handle<Object> memories_obj =
        GetOrCreateInstanceProxy<MemoriesProxy>(isolate, instance);
    result->set(length++, *memories_str);
    result->set(length++, *memories_obj);
  }

  if (TablesProxy::Count(isolate, instance) != 0) {
    Handle<String> tables_str =
        isolate->factory()->NewStringFromAsciiChecked("[[Tables]]");
    Handle<Object> tables_obj =
        GetOrCreateInstanceProxy<TablesProxy>(isolate, instance);
    result->set(length++, *tables_str);
    result->set(length++, *tables_obj);
  }

  return isolate->factory()->NewJSArrayWithElements(result, PACKED_ELEMENTS,
                                                    length);
}

Handle<JSArray> GetWasmModuleObjectInternalProperties(
    Handle<WasmModuleObject> module_object) {
  Isolate* isolate = module_object->GetIsolate();
  Handle<FixedArray> result = isolate->factory()->NewFixedArray(2 * 2);
  int length = 0;

  Handle<String> exports_str =
      isolate->factory()->NewStringFromStaticChars("[[Exports]]");
  Handle<JSArray> exports_obj = wasm::GetExports(isolate, module_object);
  result->set(length++, *exports_str);
  result->set(length++, *exports_obj);

  Handle<String> imports_str =
      isolate->factory()->NewStringFromStaticChars("[[Imports]]");
  Handle<JSArray> imports_obj = wasm::GetImports(isolate, module_object);
  result->set(length++, *imports_str);
  result->set(length++, *imports_obj);

  return isolate->factory()->NewJSArrayWithElements(result, PACKED_ELEMENTS,
                                                    length);
}

}  // namespace internal
}  // namespace v8
