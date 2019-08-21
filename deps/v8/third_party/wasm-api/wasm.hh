// WebAssembly C++ API

#ifndef __WASM_HH
#define __WASM_HH

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <string>


///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

// Machine types

static_assert(sizeof(float) == sizeof(int32_t), "incompatible float type");
static_assert(sizeof(double) == sizeof(int64_t), "incompatible double type");
static_assert(sizeof(intptr_t) == sizeof(int32_t) ||
              sizeof(intptr_t) == sizeof(int64_t), "incompatible pointer type");

using byte_t = char;
using float32_t = float;
using float64_t = double;


namespace wasm {

// Ownership

template<class T> struct owner { using type = T; };
template<class T> struct owner<T*> { using type = std::unique_ptr<T>; };

template<class T>
using own = typename owner<T>::type;

template<class T>
auto make_own(T x) -> own<T> { return own<T>(std::move(x)); }


// Vectors

template<class T>
struct vec_traits {
  static void construct(size_t size, T data[]) {}
  static void destruct(size_t size, T data[]) {}
  static void move(size_t size, T* data, T init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = std::move(init[i]);
  }
  static void copy(size_t size, T data[], const T init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i];
  }

  using proxy = T&;
};

template<class T>
struct vec_traits<T*> {
  static void construct(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) data[i] = nullptr;
  }
  static void destruct(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) {
      if (data[i]) delete data[i];
    }
  }
  static void move(size_t size, T* data[], own<T*> init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i].release();
  }
  static void copy(size_t size, T* data[], const T* const init[]) {
    for (size_t i = 0; i < size; ++i) {
      if (init[i]) data[i] = init[i]->copy().release();
    }
  }

  class proxy {
    T*& elem_;
  public:
    proxy(T*& elem) : elem_(elem) {}
    operator T*() { return elem_; }
    operator const T*() const { return elem_; }
    auto operator=(own<T*>&& elem) -> proxy& {
      reset(std::move(elem));
      return *this;
    }
    void reset(own<T*>&& val = own<T*>()) {
      if (elem_) delete elem_;
      elem_ = val.release();
    }
    auto release() -> T* {
      auto elem = elem_;
      elem_ = nullptr;
      return elem;
    }
    auto move() -> own<T*> { return make_own(release()); }
    auto get() -> T* { return elem_; }
    auto get() const -> const T* { return elem_; }
    auto operator->() -> T* { return elem_; }
    auto operator->() const -> const T* { return elem_; }
  };
};


template<class T>
class vec {
  static const size_t invalid_size = SIZE_MAX;

  size_t size_;
  std::unique_ptr<T[]> data_;

#ifdef DEBUG
  void make_data();
  void free_data();
#else
  void make_data() {}
  void free_data() {}
#endif

  vec(size_t size) : vec(size, size ? new(std::nothrow) T[size] : nullptr) {
    make_data();
  }

  vec(size_t size, T* data) : size_(size), data_(data) {
    assert(!!size_ == !!data_ || size_ == invalid_size);
  }

public:
  template<class U>
  vec(vec<U>&& that) : vec(that.size_, that.data_.release()) {}

  ~vec() {
    if (data_) vec_traits<T>::destruct(size_, data_.get());
    free_data();
  }

  operator bool() const {
    return bool(size_ != invalid_size);
  }

  auto size() const -> size_t {
    return size_;
  }

  auto get() const -> const T* {
    return data_.get();
  }

  auto get() -> T* {
    return data_.get();
  }

  auto release() -> T* {
    return data_.release();
  }

  void reset() {
    if (data_) vec_traits<T>::destruct(size_, data_.get());
    free_data();
    size_ = 0;
    data_.reset();
  }

  void reset(vec& that) {
    reset();
    size_ = that.size_;
    data_.reset(that.data_.release());
  }

  auto operator=(vec&& that) -> vec& {
    reset(that);
    return *this;
  }

  auto operator[](size_t i) -> typename vec_traits<T>::proxy {
    assert(i < size_);
    return typename vec_traits<T>::proxy(data_[i]);
  }

  auto operator[](size_t i) const -> const typename vec_traits<T>::proxy {
    assert(i < size_);
    return typename vec_traits<T>::proxy(data_[i]);
  }

  auto copy() const -> vec {
    auto v = vec(size_);
    if (v) vec_traits<T>::copy(size_, v.data_.get(), data_.get());
    return v;
  }

  static auto make_uninitialized(size_t size = 0) -> vec {
    auto v = vec(size);
    if (v) vec_traits<T>::construct(size, v.data_.get());
    return v;
  }

  static auto make(size_t size, own<T> init[]) -> vec {
    auto v = vec(size);
    if (v) vec_traits<T>::move(size, v.data_.get(), init);
    return v;
  }

  static auto make(std::string s) -> vec<char> {
    auto v = vec(s.length() + 1);
    if (v) std::strcpy(v.get(), s.data());
    return v;
  }

  static auto make() -> vec {
    return vec(0);
  }

  template<class... Ts>
  static auto make(Ts&&... args) -> vec {
    own<T> data[] = { make_own(std::move(args))... };
    return make(sizeof...(Ts), data);
  }

  static auto adopt(size_t size, T data[]) -> vec {
    return vec(size, data);
  }

  static auto invalid() -> vec {
    return vec(invalid_size, nullptr);
  }
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

class Config {
public:
  Config() = delete;
  ~Config();
  void operator delete(void*);

  static auto make() -> own<Config*>;

  // Implementations may provide custom methods for manipulating Configs.
};


// Engine

class Engine {
public:
  Engine() = delete;
  ~Engine();
  void operator delete(void*);

  static auto make(own<Config*>&& = Config::make()) -> own<Engine*>;
};


// Store

class Store {
public:
  Store() = delete;
  ~Store();
  void operator delete(void*);

  static auto make(Engine*) -> own<Store*>;
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Type attributes

enum Mutability { CONST, VAR };

struct Limits {
  uint32_t min;
  uint32_t max;

  Limits(uint32_t min, uint32_t max = std::numeric_limits<uint32_t>::max()) :
    min(min), max(max) {}
};


// Value Types

enum ValKind { I32, I64, F32, F64, ANYREF, FUNCREF };

inline bool is_num(ValKind k) { return k < ANYREF; }
inline bool is_ref(ValKind k) { return k >= ANYREF; }


class ValType {
public:
  ValType() = delete;
  ~ValType();
  void operator delete(void*);

  static auto make(ValKind) -> own<ValType*>;
  auto copy() const -> own<ValType*>;

  auto kind() const -> ValKind;
  auto is_num() const -> bool { return wasm::is_num(kind()); }
  auto is_ref() const -> bool { return wasm::is_ref(kind()); }
};


// External Types

enum ExternKind {
  EXTERN_FUNC, EXTERN_GLOBAL, EXTERN_TABLE, EXTERN_MEMORY
};

class FuncType;
class GlobalType;
class TableType;
class MemoryType;

class ExternType {
public:
  ExternType() = delete;
  ~ExternType();
  void operator delete(void*);

  auto copy() const-> own<ExternType*>;

  auto kind() const -> ExternKind;

  auto func() -> FuncType*;
  auto global() -> GlobalType*;
  auto table() -> TableType*;
  auto memory() -> MemoryType*;

  auto func() const -> const FuncType*;
  auto global() const -> const GlobalType*;
  auto table() const -> const TableType*;
  auto memory() const -> const MemoryType*;
};


// Function Types

enum class arrow { ARROW };

class FuncType : public ExternType {
public:
  FuncType() = delete;
  ~FuncType();

  static auto make(
    vec<ValType*>&& params = vec<ValType*>::make(),
    vec<ValType*>&& results = vec<ValType*>::make()
  ) -> own<FuncType*>;

  auto copy() const -> own<FuncType*>;

  auto params() const -> const vec<ValType*>&;
  auto results() const -> const vec<ValType*>&;
};


// Global Types

class GlobalType : public ExternType {
public:
  GlobalType() = delete;
  ~GlobalType();

  static auto make(own<ValType*>&&, Mutability) -> own<GlobalType*>;
  auto copy() const -> own<GlobalType*>;

  auto content() const -> const ValType*;
  auto mutability() const -> Mutability;
};


// Table Types

class TableType : public ExternType {
public:
  TableType() = delete;
  ~TableType();

  static auto make(own<ValType*>&&, Limits) -> own<TableType*>;
  auto copy() const -> own<TableType*>;

  auto element() const -> const ValType*;
  auto limits() const -> const Limits&;
};


// Memory Types

class MemoryType : public ExternType {
public:
  MemoryType() = delete;
  ~MemoryType();

  static auto make(Limits) -> own<MemoryType*>;
  auto copy() const -> own<MemoryType*>;

  auto limits() const -> const Limits&;
};


// Import Types

using Name = vec<byte_t>;

class ImportType {
public:
  ImportType() = delete;
  ~ImportType();
  void operator delete(void*);

  static auto make(Name&& module, Name&& name, own<ExternType*>&&) ->
    own<ImportType*>;
  auto copy() const -> own<ImportType*>;

  auto module() const -> const Name&;
  auto name() const -> const Name&;
  auto type() const -> const ExternType*;
};


// Export Types

class ExportType {
public:
  ExportType() = delete;
  ~ExportType();
  void operator delete(void*);

  static auto make(Name&&, own<ExternType*>&&) -> own<ExportType*>;
  auto copy() const -> own<ExportType*>;

  auto name() const -> const Name&;
  auto type() const -> const ExternType*;
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// References

class Ref {
public:
  Ref() = delete;
  ~Ref();
  void operator delete(void*);

  auto copy() const -> own<Ref*>;

  auto get_host_info() const -> void*;
  void set_host_info(void* info, void (*finalizer)(void*) = nullptr);
};


// Values

class Val {
  ValKind kind_;
  union impl {
    int32_t i32;
    int64_t i64;
    float32_t f32;
    float64_t f64;
    Ref* ref;
  } impl_;

  Val(ValKind kind, impl impl) : kind_(kind), impl_(impl) {}

public:
  Val() : kind_(ANYREF) { impl_.ref = nullptr; }
  Val(int32_t i) : kind_(I32) { impl_.i32 = i; }
  Val(int64_t i) : kind_(I64) { impl_.i64 = i; }
  Val(float32_t z) : kind_(F32) { impl_.f32 = z; }
  Val(float64_t z) : kind_(F64) { impl_.f64 = z; }
  Val(own<Ref*>&& r) : kind_(ANYREF) { impl_.ref = r.release(); }

  Val(Val&& that) : kind_(that.kind_), impl_(that.impl_) {
    if (is_ref()) that.impl_.ref = nullptr;
  }

  ~Val() {
    reset();
  }

  auto is_num() const -> bool { return wasm::is_num(kind_); }
  auto is_ref() const -> bool { return wasm::is_ref(kind_); }

  static auto i32(int32_t x) -> Val { return Val(x); }
  static auto i64(int64_t x) -> Val { return Val(x); }
  static auto f32(float32_t x) -> Val { return Val(x); }
  static auto f64(float64_t x) -> Val { return Val(x); }
  static auto ref(own<Ref*>&& x) -> Val { return Val(std::move(x)); }
  template<class T> inline static auto make(T x) -> Val;
  template<class T> inline static auto make(own<T>&& x) -> Val;

  void reset() {
    if (is_ref() && impl_.ref) {
      delete impl_.ref;
      impl_.ref = nullptr;
    }
  }

  void reset(Val& that) {
    reset();
    kind_ = that.kind_;
    impl_ = that.impl_;
    if (is_ref()) that.impl_.ref = nullptr;
  }

  auto operator=(Val&& that) -> Val& {
    reset(that);
    return *this;
  }

  auto kind() const -> ValKind { return kind_; }
  auto i32() const -> int32_t { assert(kind_ == I32); return impl_.i32; }
  auto i64() const -> int64_t { assert(kind_ == I64); return impl_.i64; }
  auto f32() const -> float32_t { assert(kind_ == F32); return impl_.f32; }
  auto f64() const -> float64_t { assert(kind_ == F64); return impl_.f64; }
  auto ref() const -> Ref* { assert(is_ref()); return impl_.ref; }
  template<class T> inline auto get() const -> T;

  auto release_ref() -> own<Ref*> {
    assert(is_ref());
    auto ref = impl_.ref;
    ref = nullptr;
    return own<Ref*>(ref);
  }

  auto copy() const -> Val {
    if (is_ref() && impl_.ref != nullptr) {
      impl impl;
      impl.ref = impl_.ref->copy().release();
      return Val(kind_, impl);
    } else {
      return Val(kind_, impl_);
    }
  }
};


template<> inline auto Val::make<int32_t>(int32_t x) -> Val { return Val(x); }
template<> inline auto Val::make<int64_t>(int64_t x) -> Val { return Val(x); }
template<> inline auto Val::make<float32_t>(float32_t x) -> Val { return Val(x); }
template<> inline auto Val::make<float64_t>(float64_t x) -> Val { return Val(x); }
template<> inline auto Val::make<Ref*>(own<Ref*>&& x) -> Val {
  return Val(std::move(x));
}

template<> inline auto Val::make<uint32_t>(uint32_t x) -> Val {
  return Val(static_cast<int32_t>(x));
}
template<> inline auto Val::make<uint64_t>(uint64_t x) -> Val {
  return Val(static_cast<int64_t>(x));
}

template<> inline auto Val::get<int32_t>() const -> int32_t { return i32(); }
template<> inline auto Val::get<int64_t>() const -> int64_t { return i64(); }
template<> inline auto Val::get<float32_t>() const -> float32_t { return f32(); }
template<> inline auto Val::get<float64_t>() const -> float64_t { return f64(); }
template<> inline auto Val::get<Ref*>() const -> Ref* { return ref(); }

template<> inline auto Val::get<uint32_t>() const -> uint32_t {
  return static_cast<uint32_t>(i32());
}
template<> inline auto Val::get<uint64_t>() const -> uint64_t {
  return static_cast<uint64_t>(i64());
}


// Traps

using Message = vec<byte_t>;  // null terminated

class Trap : public Ref {
public:
  Trap() = delete;
  ~Trap();

  static auto make(Store*, const Message& msg) -> own<Trap*>;
  auto copy() const -> own<Trap*>;

  auto message() const -> Message;
};


// Shared objects

template<class T>
class Shared {
public:
  Shared() = delete;
  ~Shared();
  void operator delete(void*);
};


// Modules

class Module : public Ref {
public:
  Module() = delete;
  ~Module();

  static auto validate(Store*, const vec<byte_t>& binary) -> bool;
  static auto make(Store*, const vec<byte_t>& binary) -> own<Module*>;
  auto copy() const -> own<Module*>;

  auto imports() const -> vec<ImportType*>;
  auto exports() const -> vec<ExportType*>;

  auto share() const -> own<Shared<Module>*>;
  static auto obtain(Store*, const Shared<Module>*) -> own<Module*>;

  auto serialize() const -> vec<byte_t>;
  static auto deserialize(Store*, const vec<byte_t>&) -> own<Module*>;
};


// Foreign Objects

class Foreign : public Ref {
public:
  Foreign() = delete;
  ~Foreign();

  static auto make(Store*) -> own<Foreign*>;
  auto copy() const -> own<Foreign*>;
};


// Externals

class Func;
class Global;
class Table;
class Memory;

class Extern : public Ref {
public:
  Extern() = delete;
  ~Extern();

  auto copy() const -> own<Extern*>;

  auto kind() const -> ExternKind;
  auto type() const -> own<ExternType*>;

  auto func() -> Func*;
  auto global() -> Global*;
  auto table() -> Table*;
  auto memory() -> Memory*;

  auto func() const -> const Func*;
  auto global() const -> const Global*;
  auto table() const -> const Table*;
  auto memory() const -> const Memory*;
};


// Function Instances

class Func : public Extern {
public:
  Func() = delete;
  ~Func();

  using callback = auto (*)(const Val[], Val[]) -> own<Trap*>;
  using callback_with_env = auto (*)(void*, const Val[], Val[]) -> own<Trap*>;

  static auto make(Store*, const FuncType*, callback) -> own<Func*>;
  static auto make(Store*, const FuncType*, callback_with_env,
    void*, void (*finalizer)(void*) = nullptr) -> own<Func*>;
  auto copy() const -> own<Func*>;

  auto type() const -> own<FuncType*>;
  auto param_arity() const -> size_t;
  auto result_arity() const -> size_t;

  auto call(const Val[] = nullptr, Val[] = nullptr) const -> own<Trap*>;
};


// Global Instances

class Global : public Extern {
public:
  Global() = delete;
  ~Global();

  static auto make(Store*, const GlobalType*, const Val&) -> own<Global*>;
  auto copy() const -> own<Global*>;

  auto type() const -> own<GlobalType*>;
  auto get() const -> Val;
  void set(const Val&);
};


// Table Instances

class Table : public Extern {
public:
  Table() = delete;
  ~Table();

  using size_t = uint32_t;

  static auto make(
    Store*, const TableType*, const Ref* init = nullptr) -> own<Table*>;
  auto copy() const -> own<Table*>;

  auto type() const -> own<TableType*>;
  auto get(size_t index) const -> own<Ref*>;
  auto set(size_t index, const Ref*) -> bool;
  auto size() const -> size_t;
  auto grow(size_t delta, const Ref* init = nullptr) -> bool;
};


// Memory Instances

class Memory : public Extern {
public:
  Memory() = delete;
  ~Memory();

  static auto make(Store*, const MemoryType*) -> own<Memory*>;
  auto copy() const -> own<Memory*>;

  using pages_t = uint32_t;

  static const size_t page_size = 0x10000;

  auto type() const -> own<MemoryType*>;
  auto data() const -> byte_t*;
  auto data_size() const -> size_t;
  auto size() const -> pages_t;
  auto grow(pages_t delta) -> bool;
};


// Module Instances

class Instance : public Ref {
public:
  Instance() = delete;
  ~Instance();

  static auto make(
    Store*, const Module*, const Extern* const[]) -> own<Instance*>;
  auto copy() const -> own<Instance*>;

  auto exports() const -> vec<Extern*>;
};


///////////////////////////////////////////////////////////////////////////////

}  // namespave wasm

#endif  // #ifdef __WASM_HH
