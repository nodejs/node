// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implementation is originally from
// https://github.com/WebAssembly/wasm-c-api/:

// Copyright 2019 Andreas Rossberg
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstring>
#include <iostream>

#include "src/wasm/c-api.h"

#include "third_party/wasm-api/wasm.h"

#include "include/libplatform/libplatform.h"
#include "src/api/api-inl.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-serialization.h"

// BEGIN FILE wasm-bin.cc

namespace wasm {
namespace bin {

////////////////////////////////////////////////////////////////////////////////
// Encoding

void encode_header(char*& ptr) {
  std::memcpy(ptr,
              "\x00"
              "asm\x01\x00\x00\x00",
              8);
  ptr += 8;
}

void encode_size32(char*& ptr, size_t n) {
  assert(n <= 0xffffffff);
  for (int i = 0; i < 5; ++i) {
    *ptr++ = (n & 0x7f) | (i == 4 ? 0x00 : 0x80);
    n = n >> 7;
  }
}

void encode_valtype(char*& ptr, const ValType* type) {
  switch (type->kind()) {
    case I32:
      *ptr++ = 0x7f;
      break;
    case I64:
      *ptr++ = 0x7e;
      break;
    case F32:
      *ptr++ = 0x7d;
      break;
    case F64:
      *ptr++ = 0x7c;
      break;
    case FUNCREF:
      *ptr++ = 0x70;
      break;
    case ANYREF:
      *ptr++ = 0x6f;
      break;
    default:
      UNREACHABLE();
  }
}

auto zero_size(const ValType* type) -> size_t {
  switch (type->kind()) {
    case I32:
      return 1;
    case I64:
      return 1;
    case F32:
      return 4;
    case F64:
      return 8;
    case FUNCREF:
      return 0;
    case ANYREF:
      return 0;
    default:
      UNREACHABLE();
  }
}

void encode_const_zero(char*& ptr, const ValType* type) {
  switch (type->kind()) {
    case I32:
      *ptr++ = 0x41;
      break;
    case I64:
      *ptr++ = 0x42;
      break;
    case F32:
      *ptr++ = 0x43;
      break;
    case F64:
      *ptr++ = 0x44;
      break;
    default:
      UNREACHABLE();
  }
  for (size_t i = 0; i < zero_size(type); ++i) *ptr++ = 0;
}

auto wrapper(const FuncType* type) -> vec<byte_t> {
  auto in_arity = type->params().size();
  auto out_arity = type->results().size();
  auto size = 39 + in_arity + out_arity;
  auto binary = vec<byte_t>::make_uninitialized(size);
  auto ptr = binary.get();

  encode_header(ptr);

  *ptr++ = i::wasm::kTypeSectionCode;
  encode_size32(ptr, 12 + in_arity + out_arity);  // size
  *ptr++ = 1;                                     // length
  *ptr++ = i::wasm::kWasmFunctionTypeCode;
  encode_size32(ptr, in_arity);
  for (size_t i = 0; i < in_arity; ++i) {
    encode_valtype(ptr, type->params()[i].get());
  }
  encode_size32(ptr, out_arity);
  for (size_t i = 0; i < out_arity; ++i) {
    encode_valtype(ptr, type->results()[i].get());
  }

  *ptr++ = i::wasm::kImportSectionCode;
  *ptr++ = 5;  // size
  *ptr++ = 1;  // length
  *ptr++ = 0;  // module length
  *ptr++ = 0;  // name length
  *ptr++ = i::wasm::kExternalFunction;
  *ptr++ = 0;  // type index

  *ptr++ = i::wasm::kExportSectionCode;
  *ptr++ = 4;  // size
  *ptr++ = 1;  // length
  *ptr++ = 0;  // name length
  *ptr++ = i::wasm::kExternalFunction;
  *ptr++ = 0;  // func index

  assert(ptr - binary.get() == static_cast<ptrdiff_t>(size));
  return binary;
}

////////////////////////////////////////////////////////////////////////////////
// Decoding

// Numbers

auto u32(const byte_t*& pos) -> uint32_t {
  uint32_t n = 0;
  uint32_t shift = 0;
  byte_t b;
  do {
    b = *pos++;
    n += (b & 0x7f) << shift;
    shift += 7;
  } while ((b & 0x80) != 0);
  return n;
}

auto u64(const byte_t*& pos) -> uint64_t {
  uint64_t n = 0;
  uint64_t shift = 0;
  byte_t b;
  do {
    b = *pos++;
    n += (b & 0x7f) << shift;
    shift += 7;
  } while ((b & 0x80) != 0);
  return n;
}

void u32_skip(const byte_t*& pos) { bin::u32(pos); }

// Names

auto name(const byte_t*& pos) -> Name {
  auto size = bin::u32(pos);
  auto start = pos;
  auto name = Name::make_uninitialized(size);
  std::memcpy(name.get(), start, size);
  pos += size;
  return name;
}

// Types

auto valtype(const byte_t*& pos) -> own<wasm::ValType*> {
  switch (*pos++) {
    case i::wasm::kLocalI32:
      return ValType::make(I32);
    case i::wasm::kLocalI64:
      return ValType::make(I64);
    case i::wasm::kLocalF32:
      return ValType::make(F32);
    case i::wasm::kLocalF64:
      return ValType::make(F64);
    case i::wasm::kLocalAnyFunc:
      return ValType::make(FUNCREF);
    case i::wasm::kLocalAnyRef:
      return ValType::make(ANYREF);
    default:
      // TODO(wasm+): support new value types
      UNREACHABLE();
  }
  return {};
}

auto mutability(const byte_t*& pos) -> Mutability {
  return *pos++ ? VAR : CONST;
}

auto limits(const byte_t*& pos) -> Limits {
  auto tag = *pos++;
  auto min = bin::u32(pos);
  if ((tag & 0x01) == 0) {
    return Limits(min);
  } else {
    auto max = bin::u32(pos);
    return Limits(min, max);
  }
}

auto stacktype(const byte_t*& pos) -> vec<ValType*> {
  size_t size = bin::u32(pos);
  auto v = vec<ValType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) v[i] = bin::valtype(pos);
  return v;
}

auto functype(const byte_t*& pos) -> own<FuncType*> {
  assert(*pos == i::wasm::kWasmFunctionTypeCode);
  ++pos;
  auto params = bin::stacktype(pos);
  auto results = bin::stacktype(pos);
  return FuncType::make(std::move(params), std::move(results));
}

auto globaltype(const byte_t*& pos) -> own<GlobalType*> {
  auto content = bin::valtype(pos);
  auto mutability = bin::mutability(pos);
  return GlobalType::make(std::move(content), mutability);
}

auto tabletype(const byte_t*& pos) -> own<TableType*> {
  auto elem = bin::valtype(pos);
  auto limits = bin::limits(pos);
  return TableType::make(std::move(elem), limits);
}

auto memorytype(const byte_t*& pos) -> own<MemoryType*> {
  auto limits = bin::limits(pos);
  return MemoryType::make(limits);
}

// Expressions

void expr_skip(const byte_t*& pos) {
  switch (*pos++) {
    case i::wasm::kExprI32Const:
    case i::wasm::kExprI64Const:
    case i::wasm::kExprGetGlobal: {
      bin::u32_skip(pos);
    } break;
    case i::wasm::kExprF32Const: {
      pos += 4;
    } break;
    case i::wasm::kExprF64Const: {
      pos += 8;
    } break;
    default: {
      // TODO(wasm+): support new expression forms
      UNREACHABLE();
    }
  }
  ++pos;  // end
}

// Sections

auto section(const vec<const byte_t>& binary, i::wasm::SectionCode sec)
    -> const byte_t* {
  const byte_t* end = binary.get() + binary.size();
  const byte_t* pos = binary.get() + 8;  // skip header
  while (pos < end && *pos++ != sec) {
    auto size = bin::u32(pos);
    pos += size;
  }
  if (pos == end) return nullptr;
  bin::u32_skip(pos);
  return pos;
}

// Only for asserts/DCHECKs.
auto section_end(const vec<const byte_t>& binary, i::wasm::SectionCode sec)
    -> const byte_t* {
  const byte_t* end = binary.get() + binary.size();
  const byte_t* pos = binary.get() + 8;  // skip header
  while (pos < end && *pos != sec) {
    ++pos;
    auto size = bin::u32(pos);
    pos += size;
  }
  if (pos == end) return nullptr;
  ++pos;
  auto size = bin::u32(pos);
  return pos + size;
}

// Type section

auto types(const vec<const byte_t>& binary) -> vec<FuncType*> {
  auto pos = bin::section(binary, i::wasm::kTypeSectionCode);
  if (pos == nullptr) return vec<FuncType*>::make();
  size_t size = bin::u32(pos);
  // TODO(wasm+): support new deftypes
  auto v = vec<FuncType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    v[i] = bin::functype(pos);
  }
  assert(pos == bin::section_end(binary, i::wasm::kTypeSectionCode));
  return v;
}

// Import section

auto imports(const vec<const byte_t>& binary, const vec<FuncType*>& types)
    -> vec<ImportType*> {
  auto pos = bin::section(binary, i::wasm::kImportSectionCode);
  if (pos == nullptr) return vec<ImportType*>::make();
  size_t size = bin::u32(pos);
  auto v = vec<ImportType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    auto module = bin::name(pos);
    auto name = bin::name(pos);
    own<ExternType*> type;
    switch (*pos++) {
      case i::wasm::kExternalFunction:
        type = types[bin::u32(pos)]->copy();
        break;
      case i::wasm::kExternalTable:
        type = bin::tabletype(pos);
        break;
      case i::wasm::kExternalMemory:
        type = bin::memorytype(pos);
        break;
      case i::wasm::kExternalGlobal:
        type = bin::globaltype(pos);
        break;
      default:
        UNREACHABLE();
    }
    v[i] =
        ImportType::make(std::move(module), std::move(name), std::move(type));
  }
  assert(pos == bin::section_end(binary, i::wasm::kImportSectionCode));
  return v;
}

auto count(const vec<ImportType*>& imports, ExternKind kind) -> uint32_t {
  uint32_t n = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    if (imports[i]->type()->kind() == kind) ++n;
  }
  return n;
}

// Function section

auto funcs(const vec<const byte_t>& binary, const vec<ImportType*>& imports,
           const vec<FuncType*>& types) -> vec<FuncType*> {
  auto pos = bin::section(binary, i::wasm::kFunctionSectionCode);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v =
      vec<FuncType*>::make_uninitialized(size + count(imports, EXTERN_FUNC));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto et = imports[i]->type();
    if (et->kind() == EXTERN_FUNC) {
      v[j++] = et->func()->copy();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = types[bin::u32(pos)]->copy();
    }
    assert(pos == bin::section_end(binary, i::wasm::kFunctionSectionCode));
  }
  return v;
}

// Global section

auto globals(const vec<const byte_t>& binary, const vec<ImportType*>& imports)
    -> vec<GlobalType*> {
  auto pos = bin::section(binary, i::wasm::kGlobalSectionCode);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<GlobalType*>::make_uninitialized(size +
                                                count(imports, EXTERN_GLOBAL));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto et = imports[i]->type();
    if (et->kind() == EXTERN_GLOBAL) {
      v[j++] = et->global()->copy();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::globaltype(pos);
      expr_skip(pos);
    }
    assert(pos == bin::section_end(binary, i::wasm::kGlobalSectionCode));
  }
  return v;
}

// Table section

auto tables(const vec<const byte_t>& binary, const vec<ImportType*>& imports)
    -> vec<TableType*> {
  auto pos = bin::section(binary, i::wasm::kTableSectionCode);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v =
      vec<TableType*>::make_uninitialized(size + count(imports, EXTERN_TABLE));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto et = imports[i]->type();
    if (et->kind() == EXTERN_TABLE) {
      v[j++] = et->table()->copy();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::tabletype(pos);
    }
    assert(pos == bin::section_end(binary, i::wasm::kTableSectionCode));
  }
  return v;
}

// Memory section

auto memories(const vec<const byte_t>& binary, const vec<ImportType*>& imports)
    -> vec<MemoryType*> {
  auto pos = bin::section(binary, i::wasm::kMemorySectionCode);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<MemoryType*>::make_uninitialized(size +
                                                count(imports, EXTERN_MEMORY));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto et = imports[i]->type();
    if (et->kind() == EXTERN_MEMORY) {
      v[j++] = et->memory()->copy();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::memorytype(pos);
    }
    assert(pos == bin::section_end(binary, i::wasm::kMemorySectionCode));
  }
  return v;
}

// Export section

auto exports(const vec<const byte_t>& binary, const vec<FuncType*>& funcs,
             const vec<GlobalType*>& globals, const vec<TableType*>& tables,
             const vec<MemoryType*>& memories) -> vec<ExportType*> {
  auto pos = bin::section(binary, i::wasm::kExportSectionCode);
  if (pos == nullptr) return vec<ExportType*>::make();
  size_t size = bin::u32(pos);
  auto exports = vec<ExportType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    auto name = bin::name(pos);
    auto tag = *pos++;
    auto index = bin::u32(pos);
    own<ExternType*> type;
    switch (tag) {
      case i::wasm::kExternalFunction:
        type = funcs[index]->copy();
        break;
      case i::wasm::kExternalTable:
        type = tables[index]->copy();
        break;
      case i::wasm::kExternalMemory:
        type = memories[index]->copy();
        break;
      case i::wasm::kExternalGlobal:
        type = globals[index]->copy();
        break;
      default:
        UNREACHABLE();
    }
    exports[i] = ExportType::make(std::move(name), std::move(type));
  }
  assert(pos == bin::section_end(binary, i::wasm::kExportSectionCode));
  return exports;
}

auto imports(const vec<const byte_t>& binary) -> vec<ImportType*> {
  return bin::imports(binary, bin::types(binary));
}

auto exports(const vec<const byte_t>& binary) -> vec<ExportType*> {
  auto types = bin::types(binary);
  auto imports = bin::imports(binary, types);
  auto funcs = bin::funcs(binary, imports, types);
  auto globals = bin::globals(binary, imports);
  auto tables = bin::tables(binary, imports);
  auto memories = bin::memories(binary, imports);
  return bin::exports(binary, funcs, globals, tables, memories);
}

}  // namespace bin
}  // namespace wasm

// BEGIN FILE wasm-v8-lowlevel.cc

namespace v8 {
namespace wasm {

// Foreign pointers

auto foreign_new(v8::Isolate* isolate, void* ptr) -> v8::Local<v8::Value> {
  auto foreign = v8::FromCData(reinterpret_cast<i::Isolate*>(isolate),
                               reinterpret_cast<i::Address>(ptr));
  return v8::Utils::ToLocal(foreign);
}

auto foreign_get(v8::Local<v8::Value> val) -> void* {
  auto foreign = v8::Utils::OpenHandle(*val);
  if (!foreign->IsForeign()) return nullptr;
  auto addr = v8::ToCData<i::Address>(*foreign);
  return reinterpret_cast<void*>(addr);
}

// Types

auto v8_valtype_to_wasm(i::wasm::ValueType v8_valtype) -> ::wasm::ValKind {
  switch (v8_valtype) {
    case i::wasm::kWasmI32:
      return ::wasm::I32;
    case i::wasm::kWasmI64:
      return ::wasm::I64;
    case i::wasm::kWasmF32:
      return ::wasm::F32;
    case i::wasm::kWasmF64:
      return ::wasm::F64;
    default:
      // TODO(wasm+): support new value types
      UNREACHABLE();
  }
}

i::wasm::ValueType wasm_valtype_to_v8(::wasm::ValKind type) {
  switch (type) {
    case ::wasm::I32:
      return i::wasm::kWasmI32;
    case ::wasm::I64:
      return i::wasm::kWasmI64;
    case ::wasm::F32:
      return i::wasm::kWasmF32;
    case ::wasm::F64:
      return i::wasm::kWasmF64;
    default:
      // TODO(wasm+): support new value types
      UNREACHABLE();
  }
}

}  // namespace wasm
}  // namespace v8

/// BEGIN FILE wasm-v8.cc

namespace wasm {

///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

[[noreturn]] void WASM_UNIMPLEMENTED(const char* s) {
  std::cerr << "Wasm API: " << s << " not supported yet!\n";
  exit(1);
}

template <class T>
void ignore(T) {}

template <class C>
struct implement;

template <class C>
auto impl(C* x) -> typename implement<C>::type* {
  return reinterpret_cast<typename implement<C>::type*>(x);
}

template <class C>
auto impl(const C* x) -> const typename implement<C>::type* {
  return reinterpret_cast<const typename implement<C>::type*>(x);
}

template <class C>
auto seal(typename implement<C>::type* x) -> C* {
  return reinterpret_cast<C*>(x);
}

template <class C>
auto seal(const typename implement<C>::type* x) -> const C* {
  return reinterpret_cast<const C*>(x);
}

#ifdef DEBUG
template <class T>
void vec<T>::make_data() {}

template <class T>
void vec<T>::free_data() {}
#endif

///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

struct ConfigImpl {
  ConfigImpl() {}
  ~ConfigImpl() {}
};

template <>
struct implement<Config> {
  using type = ConfigImpl;
};

Config::~Config() { impl(this)->~ConfigImpl(); }

void Config::operator delete(void* p) { ::operator delete(p); }

auto Config::make() -> own<Config*> {
  return own<Config*>(seal<Config>(new (std::nothrow) ConfigImpl()));
}

// Engine

struct EngineImpl {
  static bool created;

  std::unique_ptr<v8::Platform> platform;

  EngineImpl() {
    assert(!created);
    created = true;
  }

  ~EngineImpl() {
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
  }
};

bool EngineImpl::created = false;

template <>
struct implement<Engine> {
  using type = EngineImpl;
};

Engine::~Engine() { impl(this)->~EngineImpl(); }

void Engine::operator delete(void* p) { ::operator delete(p); }

auto Engine::make(own<Config*>&& config) -> own<Engine*> {
  i::FLAG_expose_gc = true;
  i::FLAG_experimental_wasm_bigint = true;
  i::FLAG_experimental_wasm_mv = true;
  auto engine = new (std::nothrow) EngineImpl;
  if (!engine) return own<Engine*>();
  engine->platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(engine->platform.get());
  v8::V8::Initialize();
  return make_own(seal<Engine>(engine));
}

// Stores

StoreImpl::~StoreImpl() {
#ifdef DEBUG
  reinterpret_cast<i::Isolate*>(isolate_)->heap()->PreciseCollectAllGarbage(
      i::Heap::kNoGCFlags, i::GarbageCollectionReason::kTesting,
      v8::kGCCallbackFlagForced);
#endif
  context()->Exit();
  isolate_->Exit();
  isolate_->Dispose();
  delete create_params_.array_buffer_allocator;
}

template <>
struct implement<Store> {
  using type = StoreImpl;
};

Store::~Store() { impl(this)->~StoreImpl(); }

void Store::operator delete(void* p) { ::operator delete(p); }

auto Store::make(Engine*) -> own<Store*> {
  auto store = make_own(new (std::nothrow) StoreImpl());
  if (!store) return own<Store*>();

  // Create isolate.
  store->create_params_.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  auto isolate = v8::Isolate::New(store->create_params_);
  if (!isolate) return own<Store*>();

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    // Create context.
    auto context = v8::Context::New(isolate);
    if (context.IsEmpty()) return own<Store*>();
    v8::Context::Scope context_scope(context);

    store->isolate_ = isolate;
    store->context_ = v8::Eternal<v8::Context>(isolate, context);
  }

  store->isolate()->Enter();
  store->context()->Enter();
  isolate->SetData(0, store.get());

  return make_own(seal<Store>(store.release()));
}

///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Value Types

struct ValTypeImpl {
  ValKind kind;

  explicit ValTypeImpl(ValKind kind) : kind(kind) {}
};

template <>
struct implement<ValType> {
  using type = ValTypeImpl;
};

ValTypeImpl* valtypes[] = {
    new ValTypeImpl(I32), new ValTypeImpl(I64),    new ValTypeImpl(F32),
    new ValTypeImpl(F64), new ValTypeImpl(ANYREF), new ValTypeImpl(FUNCREF),
};

ValType::~ValType() {}

void ValType::operator delete(void*) {}

auto ValType::make(ValKind k) -> own<ValType*> {
  auto result = seal<ValType>(valtypes[k]);
  return own<ValType*>(result);
}

auto ValType::copy() const -> own<ValType*> { return make(kind()); }

auto ValType::kind() const -> ValKind { return impl(this)->kind; }

// Extern Types

struct ExternTypeImpl {
  ExternKind kind;

  explicit ExternTypeImpl(ExternKind kind) : kind(kind) {}
  virtual ~ExternTypeImpl() {}
};

template <>
struct implement<ExternType> {
  using type = ExternTypeImpl;
};

ExternType::~ExternType() { impl(this)->~ExternTypeImpl(); }

void ExternType::operator delete(void* p) { ::operator delete(p); }

auto ExternType::copy() const -> own<ExternType*> {
  switch (kind()) {
    case EXTERN_FUNC:
      return func()->copy();
    case EXTERN_GLOBAL:
      return global()->copy();
    case EXTERN_TABLE:
      return table()->copy();
    case EXTERN_MEMORY:
      return memory()->copy();
  }
}

auto ExternType::kind() const -> ExternKind { return impl(this)->kind; }

// Function Types

struct FuncTypeImpl : ExternTypeImpl {
  vec<ValType*> params;
  vec<ValType*> results;

  FuncTypeImpl(vec<ValType*>& params, vec<ValType*>& results)
      : ExternTypeImpl(EXTERN_FUNC),
        params(std::move(params)),
        results(std::move(results)) {}

  ~FuncTypeImpl() {}
};

template <>
struct implement<FuncType> {
  using type = FuncTypeImpl;
};

FuncType::~FuncType() {}

auto FuncType::make(vec<ValType*>&& params, vec<ValType*>&& results)
    -> own<FuncType*> {
  return params && results
             ? own<FuncType*>(seal<FuncType>(new (std::nothrow)
                                                 FuncTypeImpl(params, results)))
             : own<FuncType*>();
}

auto FuncType::copy() const -> own<FuncType*> {
  return make(params().copy(), results().copy());
}

auto FuncType::params() const -> const vec<ValType*>& {
  return impl(this)->params;
}

auto FuncType::results() const -> const vec<ValType*>& {
  return impl(this)->results;
}

auto ExternType::func() -> FuncType* {
  return kind() == EXTERN_FUNC
             ? seal<FuncType>(static_cast<FuncTypeImpl*>(impl(this)))
             : nullptr;
}

auto ExternType::func() const -> const FuncType* {
  return kind() == EXTERN_FUNC
             ? seal<FuncType>(static_cast<const FuncTypeImpl*>(impl(this)))
             : nullptr;
}

// Global Types

struct GlobalTypeImpl : ExternTypeImpl {
  own<ValType*> content;
  Mutability mutability;

  GlobalTypeImpl(own<ValType*>& content, Mutability mutability)
      : ExternTypeImpl(EXTERN_GLOBAL),
        content(std::move(content)),
        mutability(mutability) {}

  ~GlobalTypeImpl() {}
};

template <>
struct implement<GlobalType> {
  using type = GlobalTypeImpl;
};

GlobalType::~GlobalType() {}

auto GlobalType::make(own<ValType*>&& content, Mutability mutability)
    -> own<GlobalType*> {
  return content ? own<GlobalType*>(seal<GlobalType>(
                       new (std::nothrow) GlobalTypeImpl(content, mutability)))
                 : own<GlobalType*>();
}

auto GlobalType::copy() const -> own<GlobalType*> {
  return make(content()->copy(), mutability());
}

auto GlobalType::content() const -> const ValType* {
  return impl(this)->content.get();
}

auto GlobalType::mutability() const -> Mutability {
  return impl(this)->mutability;
}

auto ExternType::global() -> GlobalType* {
  return kind() == EXTERN_GLOBAL
             ? seal<GlobalType>(static_cast<GlobalTypeImpl*>(impl(this)))
             : nullptr;
}

auto ExternType::global() const -> const GlobalType* {
  return kind() == EXTERN_GLOBAL
             ? seal<GlobalType>(static_cast<const GlobalTypeImpl*>(impl(this)))
             : nullptr;
}

// Table Types

struct TableTypeImpl : ExternTypeImpl {
  own<ValType*> element;
  Limits limits;

  TableTypeImpl(own<ValType*>& element, Limits limits)
      : ExternTypeImpl(EXTERN_TABLE),
        element(std::move(element)),
        limits(limits) {}

  ~TableTypeImpl() {}
};

template <>
struct implement<TableType> {
  using type = TableTypeImpl;
};

TableType::~TableType() {}

auto TableType::make(own<ValType*>&& element, Limits limits)
    -> own<TableType*> {
  return element ? own<TableType*>(seal<TableType>(
                       new (std::nothrow) TableTypeImpl(element, limits)))
                 : own<TableType*>();
}

auto TableType::copy() const -> own<TableType*> {
  return make(element()->copy(), limits());
}

auto TableType::element() const -> const ValType* {
  return impl(this)->element.get();
}

auto TableType::limits() const -> const Limits& { return impl(this)->limits; }

auto ExternType::table() -> TableType* {
  return kind() == EXTERN_TABLE
             ? seal<TableType>(static_cast<TableTypeImpl*>(impl(this)))
             : nullptr;
}

auto ExternType::table() const -> const TableType* {
  return kind() == EXTERN_TABLE
             ? seal<TableType>(static_cast<const TableTypeImpl*>(impl(this)))
             : nullptr;
}

// Memory Types

struct MemoryTypeImpl : ExternTypeImpl {
  Limits limits;

  explicit MemoryTypeImpl(Limits limits)
      : ExternTypeImpl(EXTERN_MEMORY), limits(limits) {}

  ~MemoryTypeImpl() {}
};

template <>
struct implement<MemoryType> {
  using type = MemoryTypeImpl;
};

MemoryType::~MemoryType() {}

auto MemoryType::make(Limits limits) -> own<MemoryType*> {
  return own<MemoryType*>(
      seal<MemoryType>(new (std::nothrow) MemoryTypeImpl(limits)));
}

auto MemoryType::copy() const -> own<MemoryType*> {
  return MemoryType::make(limits());
}

auto MemoryType::limits() const -> const Limits& { return impl(this)->limits; }

auto ExternType::memory() -> MemoryType* {
  return kind() == EXTERN_MEMORY
             ? seal<MemoryType>(static_cast<MemoryTypeImpl*>(impl(this)))
             : nullptr;
}

auto ExternType::memory() const -> const MemoryType* {
  return kind() == EXTERN_MEMORY
             ? seal<MemoryType>(static_cast<const MemoryTypeImpl*>(impl(this)))
             : nullptr;
}

// Import Types

struct ImportTypeImpl {
  Name module;
  Name name;
  own<ExternType*> type;

  ImportTypeImpl(Name& module, Name& name, own<ExternType*>& type)
      : module(std::move(module)),
        name(std::move(name)),
        type(std::move(type)) {}

  ~ImportTypeImpl() {}
};

template <>
struct implement<ImportType> {
  using type = ImportTypeImpl;
};

ImportType::~ImportType() { impl(this)->~ImportTypeImpl(); }

void ImportType::operator delete(void* p) { ::operator delete(p); }

auto ImportType::make(Name&& module, Name&& name, own<ExternType*>&& type)
    -> own<ImportType*> {
  return module && name && type
             ? own<ImportType*>(seal<ImportType>(
                   new (std::nothrow) ImportTypeImpl(module, name, type)))
             : own<ImportType*>();
}

auto ImportType::copy() const -> own<ImportType*> {
  return make(module().copy(), name().copy(), type()->copy());
}

auto ImportType::module() const -> const Name& { return impl(this)->module; }

auto ImportType::name() const -> const Name& { return impl(this)->name; }

auto ImportType::type() const -> const ExternType* {
  return impl(this)->type.get();
}

// Export Types

struct ExportTypeImpl {
  Name name;
  own<ExternType*> type;

  ExportTypeImpl(Name& name, own<ExternType*>& type)
      : name(std::move(name)), type(std::move(type)) {}

  ~ExportTypeImpl() {}
};

template <>
struct implement<ExportType> {
  using type = ExportTypeImpl;
};

ExportType::~ExportType() { impl(this)->~ExportTypeImpl(); }

void ExportType::operator delete(void* p) { ::operator delete(p); }

auto ExportType::make(Name&& name, own<ExternType*>&& type)
    -> own<ExportType*> {
  return name && type ? own<ExportType*>(seal<ExportType>(
                            new (std::nothrow) ExportTypeImpl(name, type)))
                      : own<ExportType*>();
}

auto ExportType::copy() const -> own<ExportType*> {
  return make(name().copy(), type()->copy());
}

auto ExportType::name() const -> const Name& { return impl(this)->name; }

auto ExportType::type() const -> const ExternType* {
  return impl(this)->type.get();
}

///////////////////////////////////////////////////////////////////////////////
// Conversions of values from and to V8 objects

auto val_to_v8(StoreImpl* store, const Val& v) -> v8::Local<v8::Value> {
  auto isolate = store->isolate();
  switch (v.kind()) {
    case I32:
      return v8::Integer::NewFromUnsigned(isolate, v.i32());
    case I64:
      return v8::BigInt::New(isolate, v.i64());
    case F32:
      return v8::Number::New(isolate, v.f32());
    case F64:
      return v8::Number::New(isolate, v.f64());
    case ANYREF:
    case FUNCREF: {
      if (v.ref() == nullptr) {
        return v8::Null(isolate);
      } else {
        WASM_UNIMPLEMENTED("ref value");
      }
    }
    default:
      UNREACHABLE();
  }
}

own<Val> v8_to_val(i::Isolate* isolate, i::Handle<i::Object> value,
                   ValKind kind) {
  switch (kind) {
    case I32:
      do {
        if (value->IsSmi()) return Val(i::Smi::ToInt(*value));
        if (value->IsHeapNumber()) {
          return Val(i::DoubleToInt32(i::HeapNumber::cast(*value).value()));
        }
        value = i::Object::ToInt32(isolate, value).ToHandleChecked();
        // This will loop back at most once.
      } while (true);
      UNREACHABLE();
    case I64:
      if (value->IsBigInt()) return Val(i::BigInt::cast(*value).AsInt64());
      return Val(
          i::BigInt::FromObject(isolate, value).ToHandleChecked()->AsInt64());
    case F32:
      do {
        if (value->IsSmi()) {
          return Val(static_cast<float32_t>(i::Smi::ToInt(*value)));
        }
        if (value->IsHeapNumber()) {
          return Val(i::DoubleToFloat32(i::HeapNumber::cast(*value).value()));
        }
        value = i::Object::ToNumber(isolate, value).ToHandleChecked();
        // This will loop back at most once.
      } while (true);
      UNREACHABLE();
    case F64:
      do {
        if (value->IsSmi()) {
          return Val(static_cast<float64_t>(i::Smi::ToInt(*value)));
        }
        if (value->IsHeapNumber()) {
          return Val(i::HeapNumber::cast(*value).value());
        }
        value = i::Object::ToNumber(isolate, value).ToHandleChecked();
        // This will loop back at most once.
      } while (true);
      UNREACHABLE();
    case ANYREF:
    case FUNCREF: {
      if (value->IsNull(isolate)) {
        return Val(nullptr);
      } else {
        WASM_UNIMPLEMENTED("ref value");
      }
    }
  }
}

i::Handle<i::String> VecToString(i::Isolate* isolate,
                                 const vec<byte_t>& chars) {
  return isolate->factory()
      ->NewStringFromUtf8({chars.get(), chars.size()})
      .ToHandleChecked();
}

// References

template <class Ref, class JSType>
class RefImpl {
 public:
  static own<Ref*> make(StoreImpl* store, i::Handle<JSType> obj) {
    RefImpl* self = new (std::nothrow) RefImpl();
    if (!self) return nullptr;
    i::Isolate* isolate = store->i_isolate();
    self->val_ = isolate->global_handles()->Create(*obj);
    return make_own(seal<Ref>(self));
  }

  void Reset() {
    i::GlobalHandles::Destroy(location());
    if (host_data_) {
      if (host_data_->finalizer) {
        host_data_->finalizer(host_data_->info);
      }
      delete host_data_;
    }
  }

  own<Ref*> copy() const { return make(store(), v8_object()); }

  StoreImpl* store() const { return StoreImpl::get(isolate()); }

  i::Isolate* isolate() const { return val_->GetIsolate(); }

  i::Handle<JSType> v8_object() const { return i::Handle<JSType>::cast(val_); }

  void* get_host_info() const {
    if (host_data_ == nullptr) return nullptr;
    return host_data_->info;
  }

  void set_host_info(void* info, void (*finalizer)(void*)) {
    host_data_ = new HostData(location(), info, finalizer);
    i::GlobalHandles::MakeWeak(host_data_->location, host_data_, &v8_finalizer,
                               v8::WeakCallbackType::kParameter);
  }

 private:
  struct HostData {
    HostData(i::Address* location, void* info, void (*finalizer)(void*))
        : location(location), info(info), finalizer(finalizer) {}
    i::Address* location;
    void* info;
    void (*finalizer)(void*);
  };

  RefImpl() {}

  static void v8_finalizer(const v8::WeakCallbackInfo<void>& info) {
    HostData* data = reinterpret_cast<HostData*>(info.GetParameter());
    i::GlobalHandles::Destroy(data->location);
    if (data->finalizer) (*data->finalizer)(data->info);
    delete data;
  }

  i::Address* location() const {
    return reinterpret_cast<i::Address*>(val_.address());
  }

  i::Handle<i::JSReceiver> val_;
  HostData* host_data_ = nullptr;
};

template <>
struct implement<Ref> {
  using type = RefImpl<Ref, i::JSReceiver>;
};

Ref::~Ref() {
  impl(this)->Reset();
  delete impl(this);
}

void Ref::operator delete(void* p) {}

auto Ref::copy() const -> own<Ref*> { return impl(this)->copy(); }

auto Ref::get_host_info() const -> void* { return impl(this)->get_host_info(); }

void Ref::set_host_info(void* info, void (*finalizer)(void*)) {
  impl(this)->set_host_info(info, finalizer);
}

///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Traps

template <>
struct implement<Trap> {
  using type = RefImpl<Trap, i::JSReceiver>;
};

Trap::~Trap() {}

auto Trap::copy() const -> own<Trap*> { return impl(this)->copy(); }

auto Trap::make(Store* store_abs, const Message& message) -> own<Trap*> {
  auto store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::String> string = VecToString(isolate, message);
  i::Handle<i::JSReceiver> exception = i::Handle<i::JSReceiver>::cast(
      isolate->factory()->NewError(isolate->error_function(), string));
  return implement<Trap>::type::make(store, exception);
}

auto Trap::message() const -> Message {
  auto isolate = impl(this)->isolate();
  i::HandleScope handle_scope(isolate);

  i::Handle<i::JSMessageObject> message =
      isolate->CreateMessage(impl(this)->v8_object(), nullptr);
  i::Handle<i::String> result = i::MessageHandler::GetMessage(isolate, message);
  result = i::String::Flatten(isolate, result);  // For performance.
  int length = 0;
  std::unique_ptr<char[]> utf8 =
      result->ToCString(i::DISALLOW_NULLS, i::FAST_STRING_TRAVERSAL, &length);
  return vec<byte_t>::adopt(length, utf8.release());
}

// Foreign Objects

template <>
struct implement<Foreign> {
  using type = RefImpl<Foreign, i::JSReceiver>;
};

Foreign::~Foreign() {}

auto Foreign::copy() const -> own<Foreign*> { return impl(this)->copy(); }

auto Foreign::make(Store* store_abs) -> own<Foreign*> {
  auto store = impl(store_abs);
  auto isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);

  auto obj = i::Handle<i::JSReceiver>();
  return implement<Foreign>::type::make(store, obj);
}

// Modules

template <>
struct implement<Module> {
  using type = RefImpl<Module, i::WasmModuleObject>;
};

Module::~Module() {}

auto Module::copy() const -> own<Module*> { return impl(this)->copy(); }

auto Module::validate(Store* store_abs, const vec<byte_t>& binary) -> bool {
  i::wasm::ModuleWireBytes bytes(
      {reinterpret_cast<const uint8_t*>(binary.get()), binary.size()});
  i::Isolate* isolate = impl(store_abs)->i_isolate();
  i::wasm::WasmFeatures features = i::wasm::WasmFeaturesFromIsolate(isolate);
  return isolate->wasm_engine()->SyncValidate(isolate, features, bytes);
}

class NopErrorThrower : public i::wasm::ErrorThrower {
 public:
  explicit NopErrorThrower(i::Isolate* isolate)
      : i::wasm::ErrorThrower(isolate, "ignored") {}
  ~NopErrorThrower() { Reset(); }
};

auto Module::make(Store* store_abs, const vec<byte_t>& binary) -> own<Module*> {
  StoreImpl* store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope scope(isolate);
  i::wasm::ModuleWireBytes bytes(
      {reinterpret_cast<const uint8_t*>(binary.get()), binary.size()});
  i::wasm::WasmFeatures features = i::wasm::WasmFeaturesFromIsolate(isolate);
  NopErrorThrower thrower(isolate);
  i::Handle<i::WasmModuleObject> module;
  if (!isolate->wasm_engine()
           ->SyncCompile(isolate, features, &thrower, bytes)
           .ToHandle(&module)) {
    return nullptr;
  }
  return implement<Module>::type::make(store, module);
}

auto Module::imports() const -> vec<ImportType*> {
  i::Vector<const uint8_t> wire_bytes =
      impl(this)->v8_object()->native_module()->wire_bytes();
  vec<const byte_t> binary = vec<const byte_t>::adopt(
      wire_bytes.size(), reinterpret_cast<const byte_t*>(wire_bytes.begin()));
  auto imports = wasm::bin::imports(binary);
  binary.release();
  return imports;
}

vec<ExportType*> ExportsImpl(i::Handle<i::WasmModuleObject> module_obj) {
  i::Vector<const uint8_t> wire_bytes =
      module_obj->native_module()->wire_bytes();
  vec<const byte_t> binary = vec<const byte_t>::adopt(
      wire_bytes.size(), reinterpret_cast<const byte_t*>(wire_bytes.begin()));
  auto exports = wasm::bin::exports(binary);
  binary.release();
  return exports;
}

auto Module::exports() const -> vec<ExportType*> {
  return ExportsImpl(impl(this)->v8_object());
}

auto Module::serialize() const -> vec<byte_t> {
  i::wasm::NativeModule* native_module =
      impl(this)->v8_object()->native_module();
  i::Vector<const uint8_t> wire_bytes = native_module->wire_bytes();
  size_t binary_size = wire_bytes.size();
  i::wasm::WasmSerializer serializer(native_module);
  size_t serial_size = serializer.GetSerializedNativeModuleSize();
  size_t size_size = i::wasm::LEBHelper::sizeof_u64v(binary_size);
  vec<byte_t> buffer =
      vec<byte_t>::make_uninitialized(size_size + binary_size + serial_size);
  byte_t* ptr = buffer.get();
  i::wasm::LEBHelper::write_u64v(reinterpret_cast<uint8_t**>(&ptr),
                                 binary_size);
  std::memcpy(ptr, wire_bytes.begin(), binary_size);
  ptr += binary_size;
  if (!serializer.SerializeNativeModule(
          {reinterpret_cast<uint8_t*>(ptr), serial_size})) {
    buffer.reset();
  }
  return std::move(buffer);
}

auto Module::deserialize(Store* store_abs, const vec<byte_t>& serialized)
    -> own<Module*> {
  StoreImpl* store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);
  const byte_t* ptr = serialized.get();
  uint64_t binary_size = wasm::bin::u64(ptr);
  ptrdiff_t size_size = ptr - serialized.get();
  size_t serial_size = serialized.size() - size_size - binary_size;
  i::Handle<i::WasmModuleObject> module_obj;
  size_t data_size = static_cast<size_t>(binary_size);
  if (!i::wasm::DeserializeNativeModule(
           isolate,
           {reinterpret_cast<const uint8_t*>(ptr + data_size), serial_size},
           {reinterpret_cast<const uint8_t*>(ptr), data_size})
           .ToHandle(&module_obj)) {
    return nullptr;
  }
  return implement<Module>::type::make(store, module_obj);
}

// TODO(v8): do better when V8 can do better.
template <>
struct implement<Shared<Module>> {
  using type = vec<byte_t>;
};

template <>
Shared<Module>::~Shared() {
  impl(this)->~vec();
}

template <>
void Shared<Module>::operator delete(void* p) {
  ::operator delete(p);
}

auto Module::share() const -> own<Shared<Module>*> {
  auto shared = seal<Shared<Module>>(new vec<byte_t>(serialize()));
  return make_own(shared);
}

auto Module::obtain(Store* store, const Shared<Module>* shared)
    -> own<Module*> {
  return Module::deserialize(store, *impl(shared));
}

// Externals

template <>
struct implement<Extern> {
  using type = RefImpl<Extern, i::JSReceiver>;
};

Extern::~Extern() {}

auto Extern::copy() const -> own<Extern*> { return impl(this)->copy(); }

auto Extern::kind() const -> ExternKind {
  i::Handle<i::JSReceiver> obj = impl(this)->v8_object();
  if (i::WasmExportedFunction::IsWasmExportedFunction(*obj)) {
    return wasm::EXTERN_FUNC;
  }
  if (obj->IsWasmGlobalObject()) return wasm::EXTERN_GLOBAL;
  if (obj->IsWasmTableObject()) return wasm::EXTERN_TABLE;
  if (obj->IsWasmMemoryObject()) return wasm::EXTERN_MEMORY;
  UNREACHABLE();
}

auto Extern::type() const -> own<ExternType*> {
  switch (kind()) {
    case EXTERN_FUNC:
      return func()->type();
    case EXTERN_GLOBAL:
      return global()->type();
    case EXTERN_TABLE:
      return table()->type();
    case EXTERN_MEMORY:
      return memory()->type();
  }
}

auto Extern::func() -> Func* {
  return kind() == EXTERN_FUNC ? static_cast<Func*>(this) : nullptr;
}

auto Extern::global() -> Global* {
  return kind() == EXTERN_GLOBAL ? static_cast<Global*>(this) : nullptr;
}

auto Extern::table() -> Table* {
  return kind() == EXTERN_TABLE ? static_cast<Table*>(this) : nullptr;
}

auto Extern::memory() -> Memory* {
  return kind() == EXTERN_MEMORY ? static_cast<Memory*>(this) : nullptr;
}

auto Extern::func() const -> const Func* {
  return kind() == EXTERN_FUNC ? static_cast<const Func*>(this) : nullptr;
}

auto Extern::global() const -> const Global* {
  return kind() == EXTERN_GLOBAL ? static_cast<const Global*>(this) : nullptr;
}

auto Extern::table() const -> const Table* {
  return kind() == EXTERN_TABLE ? static_cast<const Table*>(this) : nullptr;
}

auto Extern::memory() const -> const Memory* {
  return kind() == EXTERN_MEMORY ? static_cast<const Memory*>(this) : nullptr;
}

auto extern_to_v8(const Extern* ex) -> i::Handle<i::JSReceiver> {
  return impl(ex)->v8_object();
}

// Function Instances

template <>
struct implement<Func> {
  using type = RefImpl<Func, i::JSFunction>;
};

Func::~Func() {}

auto Func::copy() const -> own<Func*> { return impl(this)->copy(); }

struct FuncData {
  Store* store;
  own<FuncType*> type;
  enum Kind { kCallback, kCallbackWithEnv } kind;
  union {
    Func::callback callback;
    Func::callback_with_env callback_with_env;
  };
  void (*finalizer)(void*);
  void* env;

  FuncData(Store* store, const FuncType* type, Kind kind)
      : store(store),
        type(type->copy()),
        kind(kind),
        finalizer(nullptr),
        env(nullptr) {}

  ~FuncData() {
    if (finalizer) (*finalizer)(env);
  }

  static i::Address v8_callback(void* data, i::Address argv);
  static void finalize_func_data(void* data);
};

namespace {

// TODO(jkummerow): Generalize for WasmExportedFunction and WasmCapiFunction.
class SignatureHelper : public i::AllStatic {
 public:
  // Use an invalid type as a marker separating params and results.
  static const i::wasm::ValueType kMarker = i::wasm::kWasmStmt;

  static i::Handle<i::PodArray<i::wasm::ValueType>> Serialize(
      i::Isolate* isolate, FuncType* type) {
    int sig_size =
        static_cast<int>(type->params().size() + type->results().size() + 1);
    i::Handle<i::PodArray<i::wasm::ValueType>> sig =
        i::PodArray<i::wasm::ValueType>::New(isolate, sig_size,
                                             i::AllocationType::kOld);
    int index = 0;
    // TODO(jkummerow): Consider making vec<> range-based for-iterable.
    for (size_t i = 0; i < type->results().size(); i++) {
      sig->set(index++,
               v8::wasm::wasm_valtype_to_v8(type->results()[i]->kind()));
    }
    // {sig->set} needs to take the address of its second parameter,
    // so we can't pass in the static const kMarker directly.
    i::wasm::ValueType marker = kMarker;
    sig->set(index++, marker);
    for (size_t i = 0; i < type->params().size(); i++) {
      sig->set(index++,
               v8::wasm::wasm_valtype_to_v8(type->params()[i]->kind()));
    }
    return sig;
  }

  static own<FuncType*> Deserialize(i::PodArray<i::wasm::ValueType> sig) {
    int result_arity = ResultArity(sig);
    int param_arity = sig.length() - result_arity - 1;
    vec<ValType*> results = vec<ValType*>::make_uninitialized(result_arity);
    vec<ValType*> params = vec<ValType*>::make_uninitialized(param_arity);

    int i = 0;
    for (; i < result_arity; ++i) {
      results[i] = ValType::make(v8::wasm::v8_valtype_to_wasm(sig.get(i)));
    }
    i++;  // Skip marker.
    for (int p = 0; i < sig.length(); ++i, ++p) {
      params[p] = ValType::make(v8::wasm::v8_valtype_to_wasm(sig.get(i)));
    }
    return FuncType::make(std::move(params), std::move(results));
  }

  static int ResultArity(i::PodArray<i::wasm::ValueType> sig) {
    int count = 0;
    for (; count < sig.length(); count++) {
      if (sig.get(count) == kMarker) return count;
    }
    UNREACHABLE();
  }

  static int ParamArity(i::PodArray<i::wasm::ValueType> sig) {
    return sig.length() - ResultArity(sig) - 1;
  }

  static i::PodArray<i::wasm::ValueType> GetSig(
      i::Handle<i::JSFunction> function) {
    return i::WasmCapiFunction::cast(*function).GetSerializedSignature();
  }
};

auto make_func(Store* store_abs, FuncData* data) -> own<Func*> {
  auto store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::WasmCapiFunction> function = i::WasmCapiFunction::New(
      isolate, reinterpret_cast<i::Address>(&FuncData::v8_callback), data,
      SignatureHelper::Serialize(isolate, data->type.get()));
  auto func = implement<Func>::type::make(store, function);
  func->set_host_info(data, &FuncData::finalize_func_data);
  return func;
}

}  // namespace

auto Func::make(Store* store, const FuncType* type, Func::callback callback)
    -> own<Func*> {
  auto data = new FuncData(store, type, FuncData::kCallback);
  data->callback = callback;
  return make_func(store, data);
}

auto Func::make(Store* store, const FuncType* type, callback_with_env callback,
                void* env, void (*finalizer)(void*)) -> own<Func*> {
  auto data = new FuncData(store, type, FuncData::kCallbackWithEnv);
  data->callback_with_env = callback;
  data->env = env;
  data->finalizer = finalizer;
  return make_func(store, data);
}

auto Func::type() const -> own<FuncType*> {
  i::Handle<i::JSFunction> func = impl(this)->v8_object();
  if (i::WasmCapiFunction::IsWasmCapiFunction(*func)) {
    return SignatureHelper::Deserialize(SignatureHelper::GetSig(func));
  }
  DCHECK(i::WasmExportedFunction::IsWasmExportedFunction(*func));
  i::Handle<i::WasmExportedFunction> function =
      i::Handle<i::WasmExportedFunction>::cast(func);
  i::wasm::FunctionSig* sig =
      function->instance().module()->functions[function->function_index()].sig;
  uint32_t param_arity = static_cast<uint32_t>(sig->parameter_count());
  uint32_t result_arity = static_cast<uint32_t>(sig->return_count());
  auto params = vec<ValType*>::make_uninitialized(param_arity);
  auto results = vec<ValType*>::make_uninitialized(result_arity);

  for (size_t i = 0; i < params.size(); ++i) {
    auto kind = v8::wasm::v8_valtype_to_wasm(sig->GetParam(i));
    params[i] = ValType::make(kind);
  }
  for (size_t i = 0; i < results.size(); ++i) {
    auto kind = v8::wasm::v8_valtype_to_wasm(sig->GetReturn(i));
    results[i] = ValType::make(kind);
  }
  return FuncType::make(std::move(params), std::move(results));
}

auto Func::param_arity() const -> size_t {
  i::Handle<i::JSFunction> func = impl(this)->v8_object();
  if (i::WasmCapiFunction::IsWasmCapiFunction(*func)) {
    return SignatureHelper::ParamArity(SignatureHelper::GetSig(func));
  }
  DCHECK(i::WasmExportedFunction::IsWasmExportedFunction(*func));
  i::Handle<i::WasmExportedFunction> function =
      i::Handle<i::WasmExportedFunction>::cast(func);
  i::wasm::FunctionSig* sig =
      function->instance().module()->functions[function->function_index()].sig;
  return sig->parameter_count();
}

auto Func::result_arity() const -> size_t {
  i::Handle<i::JSFunction> func = impl(this)->v8_object();
  if (i::WasmCapiFunction::IsWasmCapiFunction(*func)) {
    return SignatureHelper::ResultArity(SignatureHelper::GetSig(func));
  }
  DCHECK(i::WasmExportedFunction::IsWasmExportedFunction(*func));
  i::Handle<i::WasmExportedFunction> function =
      i::Handle<i::WasmExportedFunction>::cast(func);
  i::wasm::FunctionSig* sig =
      function->instance().module()->functions[function->function_index()].sig;
  return sig->return_count();
}

auto Func::call(const Val args[], Val results[]) const -> own<Trap*> {
  auto func = impl(this);
  auto store = func->store();
  auto isolate = store->isolate();
  auto i_isolate = store->i_isolate();
  v8::HandleScope handle_scope(isolate);

  int num_params;
  int num_results;
  ValKind result_kind;
  i::Handle<i::JSFunction> v8_func = func->v8_object();
  if (i::WasmExportedFunction::IsWasmExportedFunction(*v8_func)) {
    i::WasmExportedFunction wef = i::WasmExportedFunction::cast(*v8_func);
    i::wasm::FunctionSig* sig =
        wef.instance().module()->functions[wef.function_index()].sig;
    num_params = static_cast<int>(sig->parameter_count());
    num_results = static_cast<int>(sig->return_count());
    if (num_results > 0) {
      result_kind = v8::wasm::v8_valtype_to_wasm(sig->GetReturn(0));
    }
#if DEBUG
    for (int i = 0; i < num_params; i++) {
      DCHECK_EQ(args[i].kind(), v8::wasm::v8_valtype_to_wasm(sig->GetParam(i)));
    }
#endif
  } else {
    DCHECK(i::WasmCapiFunction::IsWasmCapiFunction(*v8_func));
    UNIMPLEMENTED();
  }
  // TODO(rossberg): cache v8_args array per thread.
  auto v8_args = std::unique_ptr<i::Handle<i::Object>[]>(
      new (std::nothrow) i::Handle<i::Object>[num_params]);
  for (int i = 0; i < num_params; ++i) {
    v8_args[i] = v8::Utils::OpenHandle(*val_to_v8(store, args[i]));
  }

  // TODO(jkummerow): Use Execution::TryCall instead of manual TryCatch.
  v8::TryCatch handler(isolate);
  i::MaybeHandle<i::Object> maybe_val = i::Execution::Call(
      i_isolate, func->v8_object(), i_isolate->factory()->undefined_value(),
      num_params, v8_args.get());

  if (handler.HasCaught()) {
    i_isolate->OptionalRescheduleException(true);
    i::Handle<i::Object> exception =
        v8::Utils::OpenHandle(*handler.Exception());
    if (!exception->IsJSReceiver()) {
      i::MaybeHandle<i::String> maybe_string =
          i::Object::ToString(i_isolate, exception);
      i::Handle<i::String> string = maybe_string.is_null()
                                        ? i_isolate->factory()->empty_string()
                                        : maybe_string.ToHandleChecked();
      exception =
          i_isolate->factory()->NewError(i_isolate->error_function(), string);
    }
    return implement<Trap>::type::make(
        store, i::Handle<i::JSReceiver>::cast(exception));
  }

  auto val = maybe_val.ToHandleChecked();
  if (num_results == 0) {
    assert(val->IsUndefined(i_isolate));
  } else if (num_results == 1) {
    assert(!val->IsUndefined(i_isolate));
    new (&results[0]) Val(v8_to_val(i_isolate, val, result_kind));
  } else {
    WASM_UNIMPLEMENTED("multiple results");
  }
  return nullptr;
}

i::Address FuncData::v8_callback(void* data, i::Address argv) {
  FuncData* self = reinterpret_cast<FuncData*>(data);

  const vec<ValType*>& param_types = self->type->params();
  const vec<ValType*>& result_types = self->type->results();

  int num_param_types = static_cast<int>(param_types.size());
  int num_result_types = static_cast<int>(result_types.size());

  std::unique_ptr<Val[]> params(new Val[num_param_types]);
  std::unique_ptr<Val[]> results(new Val[num_result_types]);
  i::Address p = argv;
  for (int i = 0; i < num_param_types; ++i) {
    switch (param_types[i]->kind()) {
      case I32:
        params[i] = Val(i::ReadUnalignedValue<int32_t>(p));
        p += 4;
        break;
      case I64:
        params[i] = Val(i::ReadUnalignedValue<int64_t>(p));
        p += 8;
        break;
      case F32:
        params[i] = Val(i::ReadUnalignedValue<float32_t>(p));
        p += 4;
        break;
      case F64:
        params[i] = Val(i::ReadUnalignedValue<float64_t>(p));
        p += 8;
        break;
      case ANYREF:
      case FUNCREF: {
        i::Address raw = i::ReadUnalignedValue<i::Address>(p);
        p += sizeof(raw);
        if (raw == i::kNullAddress) {
          params[i] = Val(nullptr);
        } else {
          i::JSReceiver raw_obj = i::JSReceiver::cast(i::Object(raw));
          i::Handle<i::JSReceiver> obj(raw_obj, raw_obj.GetIsolate());
          params[i] = Val(implement<Ref>::type::make(impl(self->store), obj));
        }
        break;
      }
    }
  }

  own<Trap*> trap;
  if (self->kind == kCallbackWithEnv) {
    trap = self->callback_with_env(self->env, params.get(), results.get());
  } else {
    trap = self->callback(params.get(), results.get());
  }

  if (trap) {
    i::Isolate* isolate = impl(self->store)->i_isolate();
    isolate->Throw(*impl(trap.get())->v8_object());
    i::Object ex = isolate->pending_exception();
    isolate->clear_pending_exception();
    return ex.ptr();
  }

  p = argv;
  for (int i = 0; i < num_result_types; ++i) {
    switch (result_types[i]->kind()) {
      case I32:
        i::WriteUnalignedValue(p, results[i].i32());
        p += 4;
        break;
      case I64:
        i::WriteUnalignedValue(p, results[i].i64());
        p += 8;
        break;
      case F32:
        i::WriteUnalignedValue(p, results[i].f32());
        p += 4;
        break;
      case F64:
        i::WriteUnalignedValue(p, results[i].f64());
        p += 8;
        break;
      case ANYREF:
      case FUNCREF: {
        if (results[i].ref() == nullptr) {
          i::WriteUnalignedValue(p, i::kNullAddress);
        } else {
          i::WriteUnalignedValue(p, impl(results[i].ref())->v8_object()->ptr());
        }
        p += sizeof(i::Address);
        break;
      }
    }
  }
  return i::kNullAddress;
}

void FuncData::finalize_func_data(void* data) {
  delete reinterpret_cast<FuncData*>(data);
}

// Global Instances

template <>
struct implement<Global> {
  using type = RefImpl<Global, i::WasmGlobalObject>;
};

Global::~Global() {}

auto Global::copy() const -> own<Global*> { return impl(this)->copy(); }

auto Global::make(Store* store_abs, const GlobalType* type, const Val& val)
    -> own<Global*> {
  StoreImpl* store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);

  DCHECK_EQ(type->content()->kind(), val.kind());

  i::wasm::ValueType i_type =
      v8::wasm::wasm_valtype_to_v8(type->content()->kind());
  bool is_mutable = (type->mutability() == VAR);
  const int32_t offset = 0;
  i::Handle<i::WasmGlobalObject> obj =
      i::WasmGlobalObject::New(isolate, i::MaybeHandle<i::JSArrayBuffer>(),
                               i::MaybeHandle<i::FixedArray>(), i_type, offset,
                               is_mutable)
          .ToHandleChecked();

  auto global = implement<Global>::type::make(store, obj);
  assert(global);
  global->set(val);
  return global;
}

auto Global::type() const -> own<GlobalType*> {
  i::Handle<i::WasmGlobalObject> v8_global = impl(this)->v8_object();
  ValKind kind = v8::wasm::v8_valtype_to_wasm(v8_global->type());
  Mutability mutability = v8_global->is_mutable() ? VAR : CONST;
  return GlobalType::make(ValType::make(kind), mutability);
}

auto Global::get() const -> Val {
  i::Handle<i::WasmGlobalObject> v8_global = impl(this)->v8_object();
  switch (type()->content()->kind()) {
    case I32:
      return Val(v8_global->GetI32());
    case I64:
      return Val(v8_global->GetI64());
    case F32:
      return Val(v8_global->GetF32());
    case F64:
      return Val(v8_global->GetF64());
    case ANYREF:
    case FUNCREF:
      WASM_UNIMPLEMENTED("globals of reference type");
    default:
      // TODO(wasm+): support new value types
      UNREACHABLE();
  }
}

void Global::set(const Val& val) {
  i::Handle<i::WasmGlobalObject> v8_global = impl(this)->v8_object();
  switch (val.kind()) {
    case I32:
      return v8_global->SetI32(val.i32());
    case I64:
      return v8_global->SetI64(val.i64());
    case F32:
      return v8_global->SetF32(val.f32());
    case F64:
      return v8_global->SetF64(val.f64());
    case ANYREF:
    case FUNCREF:
      WASM_UNIMPLEMENTED("globals of reference type");
    default:
      // TODO(wasm+): support new value types
      UNREACHABLE();
  }
}

// Table Instances

template <>
struct implement<Table> {
  using type = RefImpl<Table, i::WasmTableObject>;
};

Table::~Table() {}

auto Table::copy() const -> own<Table*> { return impl(this)->copy(); }

auto Table::make(Store* store_abs, const TableType* type, const Ref* ref)
    -> own<Table*> {
  StoreImpl* store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope scope(isolate);
  auto enabled_features = i::wasm::WasmFeaturesFromFlags();

  // Get "element".
  i::wasm::ValueType i_type;
  switch (type->element()->kind()) {
    case FUNCREF:
      i_type = i::wasm::kWasmAnyFunc;
      break;
    case ANYREF:
      if (enabled_features.anyref) {
        i_type = i::wasm::kWasmAnyRef;
        break;
      }  // Else fall through.
      V8_FALLTHROUGH;
    default:
      UNREACHABLE();  // 'element' must be 'FUNCREF'.
      return nullptr;
  }

  const Limits& limits = type->limits();
  uint32_t minimum = limits.min;
  if (minimum > i::wasm::max_table_init_entries()) return nullptr;
  uint32_t maximum = limits.max;
  bool has_maximum = false;
  if (maximum != Limits(0).max) {
    has_maximum = true;
    if (maximum < minimum) return nullptr;
    if (maximum > i::wasm::max_table_init_entries()) return nullptr;
  }

  i::Handle<i::FixedArray> backing_store;
  i::Handle<i::WasmTableObject> table_obj = i::WasmTableObject::New(
      isolate, i_type, minimum, has_maximum, maximum, &backing_store);

  if (ref) {
    i::Handle<i::JSReceiver> init = impl(ref)->v8_object();
    DCHECK(i::wasm::max_table_init_entries() <= i::kMaxInt);
    for (int i = 0; i < static_cast<int>(minimum); i++) {
      // This doesn't call WasmTableObject::Set because the table has
      // just been created, so it can't be imported by any instances
      // yet that might require updating.
      DCHECK_EQ(table_obj->dispatch_tables().length(), 0);
      backing_store->set(i, *init);
    }
  }
  return implement<Table>::type::make(store, table_obj);
}

auto Table::type() const -> own<TableType*> {
  i::Handle<i::WasmTableObject> table = impl(this)->v8_object();
  uint32_t min = table->current_length();
  uint32_t max;
  if (!table->maximum_length().ToUint32(&max)) max = 0xFFFFFFFFu;
  // TODO(wasm+): support new element types.
  return TableType::make(ValType::make(FUNCREF), Limits(min, max));
}

auto Table::get(size_t index) const -> own<Ref*> {
  i::Handle<i::WasmTableObject> table = impl(this)->v8_object();
  if (index >= table->current_length()) return own<Ref*>();
  i::Isolate* isolate = table->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> result =
      i::WasmTableObject::Get(isolate, table, static_cast<uint32_t>(index));
  if (!result->IsJSFunction()) return own<Ref*>();
  DCHECK(i::WasmExportedFunction::IsWasmExportedFunction(*result) ||
         i::WasmCapiFunction::IsWasmCapiFunction(*result));
  // TODO(wasm+): other references
  return implement<Func>::type::make(impl(this)->store(),
                                     i::Handle<i::JSFunction>::cast(result));
}

auto Table::set(size_t index, const Ref* ref) -> bool {
  if (ref && !impl(ref)->v8_object()->IsFunction()) {
    WASM_UNIMPLEMENTED("non-function table elements");
  }
  i::Handle<i::WasmTableObject> table = impl(this)->v8_object();
  if (index >= table->current_length()) return false;
  i::Isolate* isolate = table->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> obj =
      ref ? i::Handle<i::Object>::cast(impl(ref)->v8_object())
          : i::Handle<i::Object>::cast(
                i::ReadOnlyRoots(isolate).null_value_handle());
  i::WasmTableObject::Set(isolate, table, static_cast<uint32_t>(index), obj);
  return true;
}

// TODO(jkummerow): Having Table::size_t shadowing "std" size_t is ugly.
auto Table::size() const -> size_t {
  return impl(this)->v8_object()->current_length();
}

auto Table::grow(size_t delta, const Ref* ref) -> bool {
  i::Handle<i::WasmTableObject> table = impl(this)->v8_object();
  i::Isolate* isolate = table->GetIsolate();
  i::HandleScope scope(isolate);
  i::Handle<i::Object> init_value =
      ref == nullptr
          ? i::Handle<i::Object>::cast(isolate->factory()->null_value())
          : i::Handle<i::Object>::cast(impl(ref)->v8_object());
  int result = i::WasmTableObject::Grow(
      isolate, table, static_cast<uint32_t>(delta), init_value);
  return result >= 0;
}

// Memory Instances

template <>
struct implement<Memory> {
  using type = RefImpl<Memory, i::WasmMemoryObject>;
};

Memory::~Memory() {}

auto Memory::copy() const -> own<Memory*> { return impl(this)->copy(); }

auto Memory::make(Store* store_abs, const MemoryType* type) -> own<Memory*> {
  StoreImpl* store = impl(store_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope scope(isolate);

  const Limits& limits = type->limits();
  uint32_t minimum = limits.min;
  if (minimum > i::wasm::max_mem_pages()) return nullptr;
  uint32_t maximum = limits.max;
  if (maximum != Limits(0).max) {
    if (maximum < minimum) return nullptr;
    if (maximum > i::wasm::kSpecMaxWasmMemoryPages) return nullptr;
  }
  bool is_shared = false;  // TODO(wasm+): Support shared memory.
  i::Handle<i::WasmMemoryObject> memory_obj;
  if (!i::WasmMemoryObject::New(isolate, minimum, maximum, is_shared)
           .ToHandle(&memory_obj)) {
    return own<Memory*>();
  }
  return implement<Memory>::type::make(store, memory_obj);
}

auto Memory::type() const -> own<MemoryType*> {
  i::Handle<i::WasmMemoryObject> memory = impl(this)->v8_object();
  uint32_t min = static_cast<uint32_t>(memory->array_buffer().byte_length() /
                                       i::wasm::kWasmPageSize);
  uint32_t max =
      memory->has_maximum_pages() ? memory->maximum_pages() : 0xFFFFFFFFu;
  return MemoryType::make(Limits(min, max));
}

auto Memory::data() const -> byte_t* {
  return reinterpret_cast<byte_t*>(
      impl(this)->v8_object()->array_buffer().backing_store());
}

auto Memory::data_size() const -> size_t {
  return impl(this)->v8_object()->array_buffer().byte_length();
}

auto Memory::size() const -> pages_t {
  return static_cast<pages_t>(
      impl(this)->v8_object()->array_buffer().byte_length() /
      i::wasm::kWasmPageSize);
}

auto Memory::grow(pages_t delta) -> bool {
  i::Handle<i::WasmMemoryObject> memory = impl(this)->v8_object();
  i::Isolate* isolate = memory->GetIsolate();
  i::HandleScope handle_scope(isolate);
  int32_t old = i::WasmMemoryObject::Grow(isolate, memory, delta);
  return old != -1;
}

// Module Instances

template <>
struct implement<Instance> {
  using type = RefImpl<Instance, i::WasmInstanceObject>;
};

Instance::~Instance() {}

auto Instance::copy() const -> own<Instance*> { return impl(this)->copy(); }

auto Instance::make(Store* store_abs, const Module* module_abs,
                    const Extern* const imports[]) -> own<Instance*> {
  StoreImpl* store = impl(store_abs);
  const implement<Module>::type* module = impl(module_abs);
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);

  DCHECK_EQ(module->v8_object()->GetIsolate(), isolate);

  vec<ImportType*> import_types = module_abs->imports();
  i::Handle<i::JSObject> imports_obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  for (size_t i = 0; i < import_types.size(); ++i) {
    auto type = import_types[i];
    i::Handle<i::String> module_str = VecToString(isolate, type->module());
    i::Handle<i::String> name_str = VecToString(isolate, type->name());

    i::Handle<i::JSObject> module_obj;
    i::LookupIterator module_it(isolate, imports_obj, module_str,
                                i::LookupIterator::OWN_SKIP_INTERCEPTOR);
    if (i::JSObject::HasProperty(&module_it).ToChecked()) {
      module_obj = i::Handle<i::JSObject>::cast(
          i::Object::GetProperty(&module_it).ToHandleChecked());
    } else {
      module_obj = isolate->factory()->NewJSObject(isolate->object_function());
      ignore(
          i::Object::SetProperty(isolate, imports_obj, module_str, module_obj));
    }
    ignore(i::Object::SetProperty(isolate, module_obj, name_str,
                                  impl(imports[i])->v8_object()));
  }

  NopErrorThrower thrower(isolate);
  i::Handle<i::WasmInstanceObject> instance_obj =
      isolate->wasm_engine()
          ->SyncInstantiate(isolate, &thrower, module->v8_object(), imports_obj,
                            i::MaybeHandle<i::JSArrayBuffer>())
          .ToHandleChecked();
  return implement<Instance>::type::make(store, instance_obj);
}

auto Instance::exports() const -> vec<Extern*> {
  const implement<Instance>::type* instance = impl(this);
  StoreImpl* store = instance->store();
  i::Isolate* isolate = store->i_isolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::WasmInstanceObject> instance_obj = instance->v8_object();
  i::Handle<i::WasmModuleObject> module_obj(instance_obj->module_object(),
                                            isolate);
  i::Handle<i::JSObject> exports_obj(instance_obj->exports_object(), isolate);

  vec<ExportType*> export_types = ExportsImpl(module_obj);
  vec<Extern*> exports = vec<Extern*>::make_uninitialized(export_types.size());
  if (!exports) return vec<Extern*>::invalid();

  for (size_t i = 0; i < export_types.size(); ++i) {
    auto& name = export_types[i]->name();
    i::Handle<i::String> name_str = VecToString(isolate, name);
    i::Handle<i::Object> obj =
        i::Object::GetProperty(isolate, exports_obj, name_str)
            .ToHandleChecked();

    const ExternType* type = export_types[i]->type();
    switch (type->kind()) {
      case EXTERN_FUNC: {
        DCHECK(i::WasmExportedFunction::IsWasmExportedFunction(*obj));
        exports[i].reset(implement<Func>::type::make(
            store, i::Handle<i::WasmExportedFunction>::cast(obj)));
      } break;
      case EXTERN_GLOBAL: {
        exports[i].reset(implement<Global>::type::make(
            store, i::Handle<i::WasmGlobalObject>::cast(obj)));
      } break;
      case EXTERN_TABLE: {
        exports[i].reset(implement<Table>::type::make(
            store, i::Handle<i::WasmTableObject>::cast(obj)));
      } break;
      case EXTERN_MEMORY: {
        exports[i].reset(implement<Memory>::type::make(
            store, i::Handle<i::WasmMemoryObject>::cast(obj)));
      } break;
    }
  }

  return exports;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace wasm

// BEGIN FILE wasm-c.cc

extern "C" {

///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

// Backing implementation

extern "C++" {

template <class T>
struct borrowed_vec {
  wasm::vec<T> it;
  explicit borrowed_vec(wasm::vec<T>&& v) : it(std::move(v)) {}
  borrowed_vec(borrowed_vec<T>&& that) : it(std::move(that.it)) {}
  ~borrowed_vec() { it.release(); }
};

}  // extern "C++"

#define WASM_DEFINE_OWN(name, Name)                                          \
  struct wasm_##name##_t : Name {};                                          \
                                                                             \
  void wasm_##name##_delete(wasm_##name##_t* x) { delete x; }                \
                                                                             \
  extern "C++" inline auto hide(Name* x)->wasm_##name##_t* {                 \
    return static_cast<wasm_##name##_t*>(x);                                 \
  }                                                                          \
  extern "C++" inline auto hide(const Name* x)->const wasm_##name##_t* {     \
    return static_cast<const wasm_##name##_t*>(x);                           \
  }                                                                          \
  extern "C++" inline auto reveal(wasm_##name##_t* x)->Name* { return x; }   \
  extern "C++" inline auto reveal(const wasm_##name##_t* x)->const Name* {   \
    return x;                                                                \
  }                                                                          \
  extern "C++" inline auto get(wasm::own<Name*>& x)->wasm_##name##_t* {      \
    return hide(x.get());                                                    \
  }                                                                          \
  extern "C++" inline auto get(const wasm::own<Name*>& x)                    \
      ->const wasm_##name##_t* {                                             \
    return hide(x.get());                                                    \
  }                                                                          \
  extern "C++" inline auto release(wasm::own<Name*>&& x)->wasm_##name##_t* { \
    return hide(x.release());                                                \
  }                                                                          \
  extern "C++" inline auto adopt(wasm_##name##_t* x)->wasm::own<Name*> {     \
    return make_own(x);                                                      \
  }

// Vectors

#define WASM_DEFINE_VEC_BASE(name, Name, ptr_or_none)                       \
  extern "C++" inline auto hide(wasm::vec<Name ptr_or_none>& v)             \
      ->wasm_##name##_vec_t* {                                              \
    static_assert(sizeof(wasm_##name##_vec_t) == sizeof(wasm::vec<Name>),   \
                  "C/C++ incompatibility");                                 \
    return reinterpret_cast<wasm_##name##_vec_t*>(&v);                      \
  }                                                                         \
  extern "C++" inline auto hide(const wasm::vec<Name ptr_or_none>& v)       \
      ->const wasm_##name##_vec_t* {                                        \
    static_assert(sizeof(wasm_##name##_vec_t) == sizeof(wasm::vec<Name>),   \
                  "C/C++ incompatibility");                                 \
    return reinterpret_cast<const wasm_##name##_vec_t*>(&v);                \
  }                                                                         \
  extern "C++" inline auto hide(Name ptr_or_none* v)                        \
      ->wasm_##name##_t ptr_or_none* {                                      \
    static_assert(                                                          \
        sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none),    \
        "C/C++ incompatibility");                                           \
    return reinterpret_cast<wasm_##name##_t ptr_or_none*>(v);               \
  }                                                                         \
  extern "C++" inline auto hide(Name ptr_or_none const* v)                  \
      ->wasm_##name##_t ptr_or_none const* {                                \
    static_assert(                                                          \
        sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none),    \
        "C/C++ incompatibility");                                           \
    return reinterpret_cast<wasm_##name##_t ptr_or_none const*>(v);         \
  }                                                                         \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none* v)           \
      ->Name ptr_or_none* {                                                 \
    static_assert(                                                          \
        sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none),    \
        "C/C++ incompatibility");                                           \
    return reinterpret_cast<Name ptr_or_none*>(v);                          \
  }                                                                         \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none const* v)     \
      ->Name ptr_or_none const* {                                           \
    static_assert(                                                          \
        sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none),    \
        "C/C++ incompatibility");                                           \
    return reinterpret_cast<Name ptr_or_none const*>(v);                    \
  }                                                                         \
  extern "C++" inline auto get(wasm::vec<Name ptr_or_none>& v)              \
      ->wasm_##name##_vec_t {                                               \
    wasm_##name##_vec_t v2 = {v.size(), hide(v.get())};                     \
    return v2;                                                              \
  }                                                                         \
  extern "C++" inline auto get(const wasm::vec<Name ptr_or_none>& v)        \
      ->const wasm_##name##_vec_t {                                         \
    wasm_##name##_vec_t v2 = {                                              \
        v.size(), const_cast<wasm_##name##_t ptr_or_none*>(hide(v.get()))}; \
    return v2;                                                              \
  }                                                                         \
  extern "C++" inline auto release(wasm::vec<Name ptr_or_none>&& v)         \
      ->wasm_##name##_vec_t {                                               \
    wasm_##name##_vec_t v2 = {v.size(), hide(v.release())};                 \
    return v2;                                                              \
  }                                                                         \
  extern "C++" inline auto adopt(wasm_##name##_vec_t* v)                    \
      ->wasm::vec<Name ptr_or_none> {                                       \
    return wasm::vec<Name ptr_or_none>::adopt(v->size, reveal(v->data));    \
  }                                                                         \
  extern "C++" inline auto borrow(const wasm_##name##_vec_t* v)             \
      ->borrowed_vec<Name ptr_or_none> {                                    \
    return borrowed_vec<Name ptr_or_none>(                                  \
        wasm::vec<Name ptr_or_none>::adopt(v->size, reveal(v->data)));      \
  }                                                                         \
                                                                            \
  void wasm_##name##_vec_new_uninitialized(wasm_##name##_vec_t* out,        \
                                           size_t size) {                   \
    *out = release(wasm::vec<Name ptr_or_none>::make_uninitialized(size));  \
  }                                                                         \
  void wasm_##name##_vec_new_empty(wasm_##name##_vec_t* out) {              \
    wasm_##name##_vec_new_uninitialized(out, 0);                            \
  }                                                                         \
                                                                            \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t* v) { adopt(v); }

// Vectors with no ownership management of elements
#define WASM_DEFINE_VEC_PLAIN(name, Name, ptr_or_none)                    \
  WASM_DEFINE_VEC_BASE(name, Name, ptr_or_none)                           \
                                                                          \
  void wasm_##name##_vec_new(wasm_##name##_vec_t* out, size_t size,       \
                             wasm_##name##_t ptr_or_none const data[]) {  \
    auto v2 = wasm::vec<Name ptr_or_none>::make_uninitialized(size);      \
    if (v2.size() != 0) {                                                 \
      memcpy(v2.get(), data, size * sizeof(wasm_##name##_t ptr_or_none)); \
    }                                                                     \
    *out = release(std::move(v2));                                        \
  }                                                                       \
                                                                          \
  void wasm_##name##_vec_copy(wasm_##name##_vec_t* out,                   \
                              wasm_##name##_vec_t* v) {                   \
    wasm_##name##_vec_new(out, v->size, v->data);                         \
  }

// Vectors who own their elements
#define WASM_DEFINE_VEC(name, Name, ptr_or_none)                         \
  WASM_DEFINE_VEC_BASE(name, Name, ptr_or_none)                          \
                                                                         \
  void wasm_##name##_vec_new(wasm_##name##_vec_t* out, size_t size,      \
                             wasm_##name##_t ptr_or_none const data[]) { \
    auto v2 = wasm::vec<Name ptr_or_none>::make_uninitialized(size);     \
    for (size_t i = 0; i < v2.size(); ++i) {                             \
      v2[i] = adopt(data[i]);                                            \
    }                                                                    \
    *out = release(std::move(v2));                                       \
  }                                                                      \
                                                                         \
  void wasm_##name##_vec_copy(wasm_##name##_vec_t* out,                  \
                              wasm_##name##_vec_t* v) {                  \
    auto v2 = wasm::vec<Name ptr_or_none>::make_uninitialized(v->size);  \
    for (size_t i = 0; i < v2.size(); ++i) {                             \
      v2[i] = adopt(wasm_##name##_copy(v->data[i]));                     \
    }                                                                    \
    *out = release(std::move(v2));                                       \
  }

extern "C++" {
template <class T>
inline auto is_empty(T* p) -> bool {
  return !p;
}
}

// Byte vectors

using byte = byte_t;
WASM_DEFINE_VEC_PLAIN(byte, byte, )

///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

WASM_DEFINE_OWN(config, wasm::Config)

wasm_config_t* wasm_config_new() { return release(wasm::Config::make()); }

// Engine

WASM_DEFINE_OWN(engine, wasm::Engine)

wasm_engine_t* wasm_engine_new() { return release(wasm::Engine::make()); }

wasm_engine_t* wasm_engine_new_with_config(wasm_config_t* config) {
  return release(wasm::Engine::make(adopt(config)));
}

// Stores

WASM_DEFINE_OWN(store, wasm::Store)

wasm_store_t* wasm_store_new(wasm_engine_t* engine) {
  return release(wasm::Store::make(engine));
}

///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Type attributes

extern "C++" inline auto hide(wasm::Mutability mutability)
    -> wasm_mutability_t {
  return static_cast<wasm_mutability_t>(mutability);
}

extern "C++" inline auto reveal(wasm_mutability_t mutability)
    -> wasm::Mutability {
  return static_cast<wasm::Mutability>(mutability);
}

extern "C++" inline auto hide(const wasm::Limits& limits)
    -> const wasm_limits_t* {
  return reinterpret_cast<const wasm_limits_t*>(&limits);
}

extern "C++" inline auto reveal(wasm_limits_t limits) -> wasm::Limits {
  return wasm::Limits(limits.min, limits.max);
}

extern "C++" inline auto hide(wasm::ValKind kind) -> wasm_valkind_t {
  return static_cast<wasm_valkind_t>(kind);
}

extern "C++" inline auto reveal(wasm_valkind_t kind) -> wasm::ValKind {
  return static_cast<wasm::ValKind>(kind);
}

extern "C++" inline auto hide(wasm::ExternKind kind) -> wasm_externkind_t {
  return static_cast<wasm_externkind_t>(kind);
}

extern "C++" inline auto reveal(wasm_externkind_t kind) -> wasm::ExternKind {
  return static_cast<wasm::ExternKind>(kind);
}

// Generic

#define WASM_DEFINE_TYPE(name, Name)                        \
  WASM_DEFINE_OWN(name, Name)                               \
  WASM_DEFINE_VEC(name, Name, *)                            \
                                                            \
  wasm_##name##_t* wasm_##name##_copy(wasm_##name##_t* t) { \
    return release(t->copy());                              \
  }

// Value Types

WASM_DEFINE_TYPE(valtype, wasm::ValType)

wasm_valtype_t* wasm_valtype_new(wasm_valkind_t k) {
  return release(wasm::ValType::make(reveal(k)));
}

wasm_valkind_t wasm_valtype_kind(const wasm_valtype_t* t) {
  return hide(t->kind());
}

// Function Types

WASM_DEFINE_TYPE(functype, wasm::FuncType)

wasm_functype_t* wasm_functype_new(wasm_valtype_vec_t* params,
                                   wasm_valtype_vec_t* results) {
  return release(wasm::FuncType::make(adopt(params), adopt(results)));
}

const wasm_valtype_vec_t* wasm_functype_params(const wasm_functype_t* ft) {
  return hide(ft->params());
}

const wasm_valtype_vec_t* wasm_functype_results(const wasm_functype_t* ft) {
  return hide(ft->results());
}

// Global Types

WASM_DEFINE_TYPE(globaltype, wasm::GlobalType)

wasm_globaltype_t* wasm_globaltype_new(wasm_valtype_t* content,
                                       wasm_mutability_t mutability) {
  return release(wasm::GlobalType::make(adopt(content), reveal(mutability)));
}

const wasm_valtype_t* wasm_globaltype_content(const wasm_globaltype_t* gt) {
  return hide(gt->content());
}

wasm_mutability_t wasm_globaltype_mutability(const wasm_globaltype_t* gt) {
  return hide(gt->mutability());
}

// Table Types

WASM_DEFINE_TYPE(tabletype, wasm::TableType)

wasm_tabletype_t* wasm_tabletype_new(wasm_valtype_t* element,
                                     const wasm_limits_t* limits) {
  return release(wasm::TableType::make(adopt(element), reveal(*limits)));
}

const wasm_valtype_t* wasm_tabletype_element(const wasm_tabletype_t* tt) {
  return hide(tt->element());
}

const wasm_limits_t* wasm_tabletype_limits(const wasm_tabletype_t* tt) {
  return hide(tt->limits());
}

// Memory Types

WASM_DEFINE_TYPE(memorytype, wasm::MemoryType)

wasm_memorytype_t* wasm_memorytype_new(const wasm_limits_t* limits) {
  return release(wasm::MemoryType::make(reveal(*limits)));
}

const wasm_limits_t* wasm_memorytype_limits(const wasm_memorytype_t* mt) {
  return hide(mt->limits());
}

// Extern Types

WASM_DEFINE_TYPE(externtype, wasm::ExternType)

wasm_externkind_t wasm_externtype_kind(const wasm_externtype_t* et) {
  return hide(et->kind());
}

wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t* ft) {
  return hide(static_cast<wasm::ExternType*>(ft));
}
wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t* gt) {
  return hide(static_cast<wasm::ExternType*>(gt));
}
wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t* tt) {
  return hide(static_cast<wasm::ExternType*>(tt));
}
wasm_externtype_t* wasm_memorytype_as_externtype(wasm_memorytype_t* mt) {
  return hide(static_cast<wasm::ExternType*>(mt));
}

const wasm_externtype_t* wasm_functype_as_externtype_const(
    const wasm_functype_t* ft) {
  return hide(static_cast<const wasm::ExternType*>(ft));
}
const wasm_externtype_t* wasm_globaltype_as_externtype_const(
    const wasm_globaltype_t* gt) {
  return hide(static_cast<const wasm::ExternType*>(gt));
}
const wasm_externtype_t* wasm_tabletype_as_externtype_const(
    const wasm_tabletype_t* tt) {
  return hide(static_cast<const wasm::ExternType*>(tt));
}
const wasm_externtype_t* wasm_memorytype_as_externtype_const(
    const wasm_memorytype_t* mt) {
  return hide(static_cast<const wasm::ExternType*>(mt));
}

wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_FUNC
             ? hide(static_cast<wasm::FuncType*>(reveal(et)))
             : nullptr;
}
wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_GLOBAL
             ? hide(static_cast<wasm::GlobalType*>(reveal(et)))
             : nullptr;
}
wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_TABLE
             ? hide(static_cast<wasm::TableType*>(reveal(et)))
             : nullptr;
}
wasm_memorytype_t* wasm_externtype_as_memorytype(wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_MEMORY
             ? hide(static_cast<wasm::MemoryType*>(reveal(et)))
             : nullptr;
}

const wasm_functype_t* wasm_externtype_as_functype_const(
    const wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_FUNC
             ? hide(static_cast<const wasm::FuncType*>(reveal(et)))
             : nullptr;
}
const wasm_globaltype_t* wasm_externtype_as_globaltype_const(
    const wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_GLOBAL
             ? hide(static_cast<const wasm::GlobalType*>(reveal(et)))
             : nullptr;
}
const wasm_tabletype_t* wasm_externtype_as_tabletype_const(
    const wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_TABLE
             ? hide(static_cast<const wasm::TableType*>(reveal(et)))
             : nullptr;
}
const wasm_memorytype_t* wasm_externtype_as_memorytype_const(
    const wasm_externtype_t* et) {
  return et->kind() == wasm::EXTERN_MEMORY
             ? hide(static_cast<const wasm::MemoryType*>(reveal(et)))
             : nullptr;
}

// Import Types

WASM_DEFINE_TYPE(importtype, wasm::ImportType)

wasm_importtype_t* wasm_importtype_new(wasm_name_t* module, wasm_name_t* name,
                                       wasm_externtype_t* type) {
  return release(
      wasm::ImportType::make(adopt(module), adopt(name), adopt(type)));
}

const wasm_name_t* wasm_importtype_module(const wasm_importtype_t* it) {
  return hide(it->module());
}

const wasm_name_t* wasm_importtype_name(const wasm_importtype_t* it) {
  return hide(it->name());
}

const wasm_externtype_t* wasm_importtype_type(const wasm_importtype_t* it) {
  return hide(it->type());
}

// Export Types

WASM_DEFINE_TYPE(exporttype, wasm::ExportType)

wasm_exporttype_t* wasm_exporttype_new(wasm_name_t* name,
                                       wasm_externtype_t* type) {
  return release(wasm::ExportType::make(adopt(name), adopt(type)));
}

const wasm_name_t* wasm_exporttype_name(const wasm_exporttype_t* et) {
  return hide(et->name());
}

const wasm_externtype_t* wasm_exporttype_type(const wasm_exporttype_t* et) {
  return hide(et->type());
}

///////////////////////////////////////////////////////////////////////////////
// Runtime Values

// References

#define WASM_DEFINE_REF_BASE(name, Name)                             \
  WASM_DEFINE_OWN(name, Name)                                        \
                                                                     \
  wasm_##name##_t* wasm_##name##_copy(const wasm_##name##_t* t) {    \
    return release(t->copy());                                       \
  }                                                                  \
                                                                     \
  void* wasm_##name##_get_host_info(const wasm_##name##_t* r) {      \
    return r->get_host_info();                                       \
  }                                                                  \
  void wasm_##name##_set_host_info(wasm_##name##_t* r, void* info) { \
    r->set_host_info(info);                                          \
  }                                                                  \
  void wasm_##name##_set_host_info_with_finalizer(                   \
      wasm_##name##_t* r, void* info, void (*finalizer)(void*)) {    \
    r->set_host_info(info, finalizer);                               \
  }

#define WASM_DEFINE_REF(name, Name)                                        \
  WASM_DEFINE_REF_BASE(name, Name)                                         \
                                                                           \
  wasm_ref_t* wasm_##name##_as_ref(wasm_##name##_t* r) {                   \
    return hide(static_cast<wasm::Ref*>(reveal(r)));                       \
  }                                                                        \
  wasm_##name##_t* wasm_ref_as_##name(wasm_ref_t* r) {                     \
    return hide(static_cast<Name*>(reveal(r)));                            \
  }                                                                        \
                                                                           \
  const wasm_ref_t* wasm_##name##_as_ref_const(const wasm_##name##_t* r) { \
    return hide(static_cast<const wasm::Ref*>(reveal(r)));                 \
  }                                                                        \
  const wasm_##name##_t* wasm_ref_as_##name##_const(const wasm_ref_t* r) { \
    return hide(static_cast<const Name*>(reveal(r)));                      \
  }

#define WASM_DEFINE_SHARABLE_REF(name, Name) \
  WASM_DEFINE_REF(name, Name)                \
  WASM_DEFINE_OWN(shared_##name, wasm::Shared<Name>)

WASM_DEFINE_REF_BASE(ref, wasm::Ref)

// Values

extern "C++" {

inline auto is_empty(wasm_val_t v) -> bool {
  return !is_ref(reveal(v.kind)) || !v.of.ref;
}

inline auto hide(wasm::Val v) -> wasm_val_t {
  wasm_val_t v2 = {hide(v.kind()), {}};
  switch (v.kind()) {
    case wasm::I32:
      v2.of.i32 = v.i32();
      break;
    case wasm::I64:
      v2.of.i64 = v.i64();
      break;
    case wasm::F32:
      v2.of.f32 = v.f32();
      break;
    case wasm::F64:
      v2.of.f64 = v.f64();
      break;
    case wasm::ANYREF:
    case wasm::FUNCREF:
      v2.of.ref = hide(v.ref());
      break;
    default:
      UNREACHABLE();
  }
  return v2;
}

inline auto release(wasm::Val v) -> wasm_val_t {
  wasm_val_t v2 = {hide(v.kind()), {}};
  switch (v.kind()) {
    case wasm::I32:
      v2.of.i32 = v.i32();
      break;
    case wasm::I64:
      v2.of.i64 = v.i64();
      break;
    case wasm::F32:
      v2.of.f32 = v.f32();
      break;
    case wasm::F64:
      v2.of.f64 = v.f64();
      break;
    case wasm::ANYREF:
    case wasm::FUNCREF:
      v2.of.ref = release(v.release_ref());
      break;
    default:
      UNREACHABLE();
  }
  return v2;
}

inline auto adopt(wasm_val_t v) -> wasm::Val {
  switch (reveal(v.kind)) {
    case wasm::I32:
      return wasm::Val(v.of.i32);
    case wasm::I64:
      return wasm::Val(v.of.i64);
    case wasm::F32:
      return wasm::Val(v.of.f32);
    case wasm::F64:
      return wasm::Val(v.of.f64);
    case wasm::ANYREF:
    case wasm::FUNCREF:
      return wasm::Val(adopt(v.of.ref));
    default:
      UNREACHABLE();
  }
}

struct borrowed_val {
  wasm::Val it;
  explicit borrowed_val(wasm::Val&& v) : it(std::move(v)) {}
  borrowed_val(borrowed_val&& that) : it(std::move(that.it)) {}
  ~borrowed_val() {
    if (it.is_ref()) it.release_ref();
  }
};

inline auto borrow(const wasm_val_t* v) -> borrowed_val {
  wasm::Val v2;
  switch (reveal(v->kind)) {
    case wasm::I32:
      v2 = wasm::Val(v->of.i32);
      break;
    case wasm::I64:
      v2 = wasm::Val(v->of.i64);
      break;
    case wasm::F32:
      v2 = wasm::Val(v->of.f32);
      break;
    case wasm::F64:
      v2 = wasm::Val(v->of.f64);
      break;
    case wasm::ANYREF:
    case wasm::FUNCREF:
      v2 = wasm::Val(adopt(v->of.ref));
      break;
    default:
      UNREACHABLE();
  }
  return borrowed_val(std::move(v2));
}

}  // extern "C++"

WASM_DEFINE_VEC_BASE(val, wasm::Val, )

void wasm_val_vec_new(wasm_val_vec_t* out, size_t size,
                      wasm_val_t const data[]) {
  auto v2 = wasm::vec<wasm::Val>::make_uninitialized(size);
  for (size_t i = 0; i < v2.size(); ++i) {
    v2[i] = adopt(data[i]);
  }
  *out = release(std::move(v2));
}

void wasm_val_vec_copy(wasm_val_vec_t* out, wasm_val_vec_t* v) {
  auto v2 = wasm::vec<wasm::Val>::make_uninitialized(v->size);
  for (size_t i = 0; i < v2.size(); ++i) {
    wasm_val_t val;
    wasm_val_copy(&v->data[i], &val);
    v2[i] = adopt(val);
  }
  *out = release(std::move(v2));
}

void wasm_val_delete(wasm_val_t* v) {
  if (is_ref(reveal(v->kind))) adopt(v->of.ref);
}

void wasm_val_copy(wasm_val_t* out, const wasm_val_t* v) {
  *out = *v;
  if (is_ref(reveal(v->kind))) {
    out->of.ref = release(v->of.ref->copy());
  }
}

///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Traps

WASM_DEFINE_REF(trap, wasm::Trap)

wasm_trap_t* wasm_trap_new(wasm_store_t* store, const wasm_message_t* message) {
  auto message_ = borrow(message);
  return release(wasm::Trap::make(store, message_.it));
}

void wasm_trap_message(const wasm_trap_t* trap, wasm_message_t* out) {
  *out = release(reveal(trap)->message());
}

// Foreign Objects

WASM_DEFINE_REF(foreign, wasm::Foreign)

wasm_foreign_t* wasm_foreign_new(wasm_store_t* store) {
  return release(wasm::Foreign::make(store));
}

// Modules

WASM_DEFINE_SHARABLE_REF(module, wasm::Module)

bool wasm_module_validate(wasm_store_t* store, const wasm_byte_vec_t* binary) {
  auto binary_ = borrow(binary);
  return wasm::Module::validate(store, binary_.it);
}

wasm_module_t* wasm_module_new(wasm_store_t* store,
                               const wasm_byte_vec_t* binary) {
  auto binary_ = borrow(binary);
  return release(wasm::Module::make(store, binary_.it));
}

void wasm_module_imports(const wasm_module_t* module,
                         wasm_importtype_vec_t* out) {
  *out = release(reveal(module)->imports());
}

void wasm_module_exports(const wasm_module_t* module,
                         wasm_exporttype_vec_t* out) {
  *out = release(reveal(module)->exports());
}

void wasm_module_serialize(const wasm_module_t* module, wasm_byte_vec_t* out) {
  *out = release(reveal(module)->serialize());
}

wasm_module_t* wasm_module_deserialize(wasm_store_t* store,
                                       const wasm_byte_vec_t* binary) {
  auto binary_ = borrow(binary);
  return release(wasm::Module::deserialize(store, binary_.it));
}

wasm_shared_module_t* wasm_module_share(const wasm_module_t* module) {
  return release(reveal(module)->share());
}

wasm_module_t* wasm_module_obtain(wasm_store_t* store,
                                  const wasm_shared_module_t* shared) {
  return release(wasm::Module::obtain(store, shared));
}

// Function Instances

WASM_DEFINE_REF(func, wasm::Func)

extern "C++" {

auto wasm_callback(void* env, const wasm::Val args[], wasm::Val results[])
    -> wasm::own<wasm::Trap*> {
  auto f = reinterpret_cast<wasm_func_callback_t>(env);
  return adopt(f(hide(args), hide(results)));
}

struct wasm_callback_env_t {
  wasm_func_callback_with_env_t callback;
  void* env;
  void (*finalizer)(void*);
};

auto wasm_callback_with_env(void* env, const wasm::Val args[],
                            wasm::Val results[]) -> wasm::own<wasm::Trap*> {
  auto t = static_cast<wasm_callback_env_t*>(env);
  return adopt(t->callback(t->env, hide(args), hide(results)));
}

void wasm_callback_env_finalizer(void* env) {
  auto t = static_cast<wasm_callback_env_t*>(env);
  if (t->finalizer) t->finalizer(t->env);
  delete t;
}

}  // extern "C++"

wasm_func_t* wasm_func_new(wasm_store_t* store, const wasm_functype_t* type,
                           wasm_func_callback_t callback) {
  return release(wasm::Func::make(store, type, wasm_callback,
                                  reinterpret_cast<void*>(callback)));
}

wasm_func_t* wasm_func_new_with_env(wasm_store_t* store,
                                    const wasm_functype_t* type,
                                    wasm_func_callback_with_env_t callback,
                                    void* env, void (*finalizer)(void*)) {
  auto env2 = new wasm_callback_env_t{callback, env, finalizer};
  return release(wasm::Func::make(store, type, wasm_callback_with_env, env2,
                                  wasm_callback_env_finalizer));
}

wasm_functype_t* wasm_func_type(const wasm_func_t* func) {
  return release(func->type());
}

size_t wasm_func_param_arity(const wasm_func_t* func) {
  return func->param_arity();
}

size_t wasm_func_result_arity(const wasm_func_t* func) {
  return func->result_arity();
}

wasm_trap_t* wasm_func_call(const wasm_func_t* func, const wasm_val_t args[],
                            wasm_val_t results[]) {
  return release(func->call(reveal(args), reveal(results)));
}

// Global Instances

WASM_DEFINE_REF(global, wasm::Global)

wasm_global_t* wasm_global_new(wasm_store_t* store,
                               const wasm_globaltype_t* type,
                               const wasm_val_t* val) {
  auto val_ = borrow(val);
  return release(wasm::Global::make(store, type, val_.it));
}

wasm_globaltype_t* wasm_global_type(const wasm_global_t* global) {
  return release(global->type());
}

void wasm_global_get(const wasm_global_t* global, wasm_val_t* out) {
  *out = release(global->get());
}

void wasm_global_set(wasm_global_t* global, const wasm_val_t* val) {
  auto val_ = borrow(val);
  global->set(val_.it);
}

// Table Instances

WASM_DEFINE_REF(table, wasm::Table)

wasm_table_t* wasm_table_new(wasm_store_t* store, const wasm_tabletype_t* type,
                             wasm_ref_t* ref) {
  return release(wasm::Table::make(store, type, ref));
}

wasm_tabletype_t* wasm_table_type(const wasm_table_t* table) {
  return release(table->type());
}

wasm_ref_t* wasm_table_get(const wasm_table_t* table, wasm_table_size_t index) {
  return release(table->get(index));
}

bool wasm_table_set(wasm_table_t* table, wasm_table_size_t index,
                    wasm_ref_t* ref) {
  return table->set(index, ref);
}

wasm_table_size_t wasm_table_size(const wasm_table_t* table) {
  return table->size();
}

bool wasm_table_grow(wasm_table_t* table, wasm_table_size_t delta,
                     wasm_ref_t* ref) {
  return table->grow(delta, ref);
}

// Memory Instances

WASM_DEFINE_REF(memory, wasm::Memory)

wasm_memory_t* wasm_memory_new(wasm_store_t* store,
                               const wasm_memorytype_t* type) {
  return release(wasm::Memory::make(store, type));
}

wasm_memorytype_t* wasm_memory_type(const wasm_memory_t* memory) {
  return release(memory->type());
}

wasm_byte_t* wasm_memory_data(wasm_memory_t* memory) { return memory->data(); }

size_t wasm_memory_data_size(const wasm_memory_t* memory) {
  return memory->data_size();
}

wasm_memory_pages_t wasm_memory_size(const wasm_memory_t* memory) {
  return memory->size();
}

bool wasm_memory_grow(wasm_memory_t* memory, wasm_memory_pages_t delta) {
  return memory->grow(delta);
}

// Externals

WASM_DEFINE_REF(extern, wasm::Extern)
WASM_DEFINE_VEC(extern, wasm::Extern, *)

wasm_externkind_t wasm_extern_kind(const wasm_extern_t* external) {
  return hide(external->kind());
}
wasm_externtype_t* wasm_extern_type(const wasm_extern_t* external) {
  return release(external->type());
}

wasm_extern_t* wasm_func_as_extern(wasm_func_t* func) {
  return hide(static_cast<wasm::Extern*>(reveal(func)));
}
wasm_extern_t* wasm_global_as_extern(wasm_global_t* global) {
  return hide(static_cast<wasm::Extern*>(reveal(global)));
}
wasm_extern_t* wasm_table_as_extern(wasm_table_t* table) {
  return hide(static_cast<wasm::Extern*>(reveal(table)));
}
wasm_extern_t* wasm_memory_as_extern(wasm_memory_t* memory) {
  return hide(static_cast<wasm::Extern*>(reveal(memory)));
}

const wasm_extern_t* wasm_func_as_extern_const(const wasm_func_t* func) {
  return hide(static_cast<const wasm::Extern*>(reveal(func)));
}
const wasm_extern_t* wasm_global_as_extern_const(const wasm_global_t* global) {
  return hide(static_cast<const wasm::Extern*>(reveal(global)));
}
const wasm_extern_t* wasm_table_as_extern_const(const wasm_table_t* table) {
  return hide(static_cast<const wasm::Extern*>(reveal(table)));
}
const wasm_extern_t* wasm_memory_as_extern_const(const wasm_memory_t* memory) {
  return hide(static_cast<const wasm::Extern*>(reveal(memory)));
}

wasm_func_t* wasm_extern_as_func(wasm_extern_t* external) {
  return hide(external->func());
}
wasm_global_t* wasm_extern_as_global(wasm_extern_t* external) {
  return hide(external->global());
}
wasm_table_t* wasm_extern_as_table(wasm_extern_t* external) {
  return hide(external->table());
}
wasm_memory_t* wasm_extern_as_memory(wasm_extern_t* external) {
  return hide(external->memory());
}

const wasm_func_t* wasm_extern_as_func_const(const wasm_extern_t* external) {
  return hide(external->func());
}
const wasm_global_t* wasm_extern_as_global_const(
    const wasm_extern_t* external) {
  return hide(external->global());
}
const wasm_table_t* wasm_extern_as_table_const(const wasm_extern_t* external) {
  return hide(external->table());
}
const wasm_memory_t* wasm_extern_as_memory_const(
    const wasm_extern_t* external) {
  return hide(external->memory());
}

// Module Instances

WASM_DEFINE_REF(instance, wasm::Instance)

wasm_instance_t* wasm_instance_new(wasm_store_t* store,
                                   const wasm_module_t* module,
                                   const wasm_extern_t* const imports[]) {
  return release(wasm::Instance::make(
      store, module, reinterpret_cast<const wasm::Extern* const*>(imports)));
}

void wasm_instance_exports(const wasm_instance_t* instance,
                           wasm_extern_vec_t* out) {
  *out = release(instance->exports());
}

#undef WASM_DEFINE_OWN
#undef WASM_DEFINE_VEC_BASE
#undef WASM_DEFINE_VEC_PLAIN
#undef WASM_DEFINE_VEC
#undef WASM_DEFINE_TYPE
#undef WASM_DEFINE_REF_BASE
#undef WASM_DEFINE_REF
#undef WASM_DEFINE_SHARABLE_REF

}  // extern "C"
