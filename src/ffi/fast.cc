#if HAVE_FFI

#include "ffi/fast.h"

#include "env-inl.h"
#include "ffi/jit_memory.h"
#include "ffi/types.h"
#include "node_errors.h"
#include "node_ffi.h"

#include <climits>
#include <limits>

namespace node::ffi {

using v8::CFunctionInfo;
using v8::CTypeInfo;
using v8::FastApiCallbackOptions;

bool IsTypeName(std::string_view type,
                std::initializer_list<std::string_view> names) {
  // Signature parsing accepts several public aliases for the same ABI type.
  // The fast path normalizes them by checking the original type name against
  // each alias set before selecting a FastFFIType.
  for (std::string_view name : names) {
    if (type == name) {
      return true;
    }
  }
  return false;
}

bool FastScalarTypeFromName(std::string_view type, FastFFIType* out) {
  // Scalars can be passed directly through V8 Fast API CFunction metadata.
  // Pointer-like types are represented as uintptr-sized unsigned integers;
  // JavaScript wrappers handle strings and object-to-pointer conversions.
  if (type == "void") {
    *out = FastFFIType::kVoid;
  } else if (type == "bool") {
    *out = FastFFIType::kUint8;
  } else if (IsTypeName(type, {"i8", "int8"})) {
    *out = FastFFIType::kInt8;
  } else if (IsTypeName(type, {"u8", "uint8"})) {
    *out = FastFFIType::kUint8;
  } else if (type == "char") {
    *out = CHAR_MIN < 0 ? FastFFIType::kInt8 : FastFFIType::kUint8;
  } else if (IsTypeName(type, {"i16", "int16"})) {
    *out = FastFFIType::kInt16;
  } else if (IsTypeName(type, {"u16", "uint16"})) {
    *out = FastFFIType::kUint16;
  } else if (IsTypeName(type, {"i32", "int32"})) {
    *out = FastFFIType::kInt32;
  } else if (IsTypeName(type, {"u32", "uint32"})) {
    *out = FastFFIType::kUint32;
  } else if (IsTypeName(type, {"i64", "int64"})) {
    *out = FastFFIType::kInt64;
  } else if (IsTypeName(type, {"u64", "uint64"})) {
    *out = FastFFIType::kUint64;
  } else if (IsTypeName(type, {"f32", "float", "float32"})) {
    *out = FastFFIType::kFloat32;
  } else if (IsTypeName(type, {"f64", "double", "float64"})) {
    *out = FastFFIType::kFloat64;
  } else if (IsTypeName(type, {"buffer", "arraybuffer"})) {
    *out = FastFFIType::kPointer;
  } else if (IsTypeName(type,
                        {"pointer", "ptr", "string", "str", "function"})) {
    *out = FastFFIType::kPointer;
  } else {
    return false;
  }
  return true;
}

bool FastArgTypeFromName(std::string_view type, FastFFIType* out) {
  // Buffer and ArrayBuffer parameters are special only for arguments: V8 passes
  // the original JS value so the generated trampoline can extract its backing
  // store address immediately before calling the native target.
  if (IsTypeName(type, {"buffer", "arraybuffer"})) {
    *out = FastFFIType::kBuffer;
    return true;
  }
  return FastScalarTypeFromName(type, out);
}

bool NeedsBigIntRepresentation(FastFFIType type) {
  // V8's Fast API has one int64 representation per CFunction. Any pointer or
  // 64-bit integer in the signature forces BigInt to avoid precision loss.
  return type == FastFFIType::kInt64 || type == FastFFIType::kUint64 ||
         type == FastFFIType::kPointer;
}

CTypeInfo::Type ToV8Type(FastFFIType type, bool is_return) {
  // Map the internal FFI type model onto the limited set of CFunctionInfo
  // types that V8 can call directly. Narrow integer arguments are widened in
  // the V8 signature and then narrowed again by the trampoline.
  switch (type) {
    case FastFFIType::kVoid:
      return CTypeInfo::Type::kVoid;
    case FastFFIType::kUint8:
      return CTypeInfo::Type::kUint32;
    case FastFFIType::kInt8:
    case FastFFIType::kInt16:
      return CTypeInfo::Type::kInt32;
    case FastFFIType::kUint16:
      return CTypeInfo::Type::kUint32;
    case FastFFIType::kInt32:
      return CTypeInfo::Type::kInt32;
    case FastFFIType::kUint32:
      return CTypeInfo::Type::kUint32;
    case FastFFIType::kInt64:
      return CTypeInfo::Type::kInt64;
    case FastFFIType::kUint64:
    case FastFFIType::kPointer:
      return CTypeInfo::Type::kUint64;
    case FastFFIType::kBuffer:
      return CTypeInfo::Type::kV8Value;
    case FastFFIType::kFloat32:
      return CTypeInfo::Type::kFloat32;
    case FastFFIType::kFloat64:
      return CTypeInfo::Type::kFloat64;
  }
  UNREACHABLE();
}

uintptr_t PointerFromValue(v8::Local<v8::Value> value) {
  // ArrayBufferViews point at the backing store plus their byte offset, while
  // ArrayBuffer and SharedArrayBuffer point at the start of their store.
  if (value->IsArrayBufferView()) {
    v8::Local<v8::ArrayBufferView> view = value.As<v8::ArrayBufferView>();
    void* data = view->Buffer()->Data();
    return reinterpret_cast<uintptr_t>(static_cast<char*>(data) +
                                       view->ByteOffset());
  }

  if (value->IsArrayBuffer()) {
    return reinterpret_cast<uintptr_t>(value.As<v8::ArrayBuffer>()->Data());
  }

  if (value->IsSharedArrayBuffer()) {
    std::shared_ptr<v8::BackingStore> store =
        value.As<v8::SharedArrayBuffer>()->GetBackingStore();
    return reinterpret_cast<uintptr_t>(store->Data());
  }

  return std::numeric_limits<uintptr_t>::max();
}

bool SignatureNeedsRawPointerConversions(const FFIFunction& fn) {
  // These public types accept JS values that are not raw BigInt pointers. When
  // a signature contains them, JS wraps the fast function to perform the
  // conversion before V8 enters the CFunction trampoline.
  for (const std::string& name : fn.arg_type_names) {
    if (name == "buffer" || name == "arraybuffer" || name == "string" ||
        name == "str") {
      return true;
    }
  }
  return false;
}

bool IsPointerTypeName(const std::string& name) {
  // `pointer`, `ptr`, and `function` all use the same uintptr ABI slot; only
  // the public type spelling differs.
  return name == "pointer" || name == "ptr" || name == "function";
}

bool SignatureNeedsFastBufferInvoke(const FFIFunction& fn) {
  // The secondary buffer invoke is only generated for the hot monomorphic case
  // where a single pointer-like argument can be satisfied by a Buffer or
  // ArrayBuffer without allocating or caching a BigInt pointer in JS.
  return fn.arg_type_names.size() == 1 &&
         IsPointerTypeName(fn.arg_type_names[0]);
}

std::shared_ptr<FFIFunction> CloneWithFastBufferArgNames(
    const std::shared_ptr<FFIFunction>& fn) {
  // Reuse the same native target and libffi metadata, but describe the JS
  // argument as `buffer` so CreateFastFFIMetadata() emits a trampoline that
  // receives a V8 value and calls node_ffi_fast_buffer_data().
  auto clone = std::make_shared<FFIFunction>(*fn);
  for (std::string& name : clone->arg_type_names) {
    if (IsPointerTypeName(name)) {
      name = "buffer";
    }
  }
  return clone;
}

extern "C" uintptr_t node_ffi_fast_buffer_data(v8::Local<v8::Value> value,
                                               FastApiCallbackOptions* options,
                                               uint32_t index) {
  // The generated trampoline treats this sentinel as "conversion failed" and
  // returns zero after throwing, preventing the native target from seeing an
  // invalid pointer value.
  constexpr uintptr_t kInvalidBuffer = std::numeric_limits<uintptr_t>::max();

  // Accept only memory-backed JS values in the native helper. Other pointer
  // conversions, including strings, stay in the JS wrapper so their temporary
  // lifetime is explicit.
  if (value->IsArrayBufferView()) {
    return PointerFromValue(value);
  }
  if (value->IsArrayBuffer() || value->IsSharedArrayBuffer()) {
    return PointerFromValue(value);
  }

  v8::Isolate* isolate = options != nullptr ? options->isolate : nullptr;
  if (isolate != nullptr) {
    THROW_ERR_INVALID_ARG_VALUE(
        isolate, "Argument %u must be a buffer or an ArrayBuffer", index);
  }
  return kInvalidBuffer;
}

FastFFIMetadata::~FastFFIMetadata() {
  // Metadata owns executable memory through `trampoline`; releasing it here
  // ties code lifetime to the V8 function's weak FFIFunctionInfo cleanup.
  node_ffi_free_fast_trampoline(&trampoline);
}

bool IsFastCallSupported() {
  // Fast call requires both a platform stub emitter and working JIT memory.
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__x86_64__) ||        \
    defined(_M_X64) || defined(__powerpc64__) || defined(__ppc64__) ||         \
    defined(__PPC64__) || defined(__loongarch64) ||                            \
    (defined(__riscv) && __riscv_xlen == 64) || defined(__s390x__)
  return IsJitMemorySupported();
#else
  return false;
#endif
}

std::unique_ptr<FastFFIMetadata> CreateFastFFIMetadata(const FFIFunction& fn) {
  // Bail early if executable memory allocation doesn't work on this process
  // (missing MAP_JIT entitlement, hardened runtime, SELinux execmem, etc.).
  // The self-test runs once and caches the result.
  if (!IsJitMemorySupported()) {
    return nullptr;
  }

  // Check signature-level eligibility (type checks, register caps, platform
  // support). Returning nullptr here lets the caller fall back to SharedBuffer
  // or the generic libffi path.
  const char* eligibility_reason;
  if (!IsFastCallEligible(fn, &eligibility_reason)) {
    return nullptr;
  }

  // Reject unsupported result types first. Returning nullptr means the caller
  // can still fall back to SharedBuffer or the generic libffi path.
  FastFFIType result;
  if (!FastScalarTypeFromName(fn.return_type_name, &result)) {
    return nullptr;
  }
  if (fn.args.size() != fn.arg_type_names.size()) {
    return nullptr;
  }
  // Keep the initial Fast API implementation bounded to signatures V8 and the
  // platform trampolines can describe without stack argument support.
  if (fn.arg_type_names.size() > 8) {
    return nullptr;
  }

  std::vector<FastFFIType> args;
  args.reserve(fn.arg_type_names.size());
  bool needs_bigint = NeedsBigIntRepresentation(result);
  bool needs_callback_options = false;
  // Normalize public argument names into FastFFIType values while collecting
  // signature-wide flags required by V8 CFunctionInfo.
  for (const std::string& name : fn.arg_type_names) {
    FastFFIType type;
    if (!FastArgTypeFromName(name, &type)) {
      return nullptr;
    }
    if (type == FastFFIType::kVoid) {
      return nullptr;
    }
    needs_bigint = needs_bigint || NeedsBigIntRepresentation(type);
    needs_callback_options =
        needs_callback_options || type == FastFFIType::kBuffer;
    args.push_back(type);
  }

  auto metadata = std::make_unique<FastFFIMetadata>();
  // The platform-specific trampoline is the executable entrypoint V8 calls.
  // If the platform rejects the signature, the whole fast metadata object is
  // discarded and the caller chooses another invocation path.
  if (!node_ffi_create_fast_trampoline(
          fn.ptr, args.data(), args.size(), result, &metadata->trampoline)) {
    return nullptr;
  }

  metadata->arg_info.reserve(args.size() + 1);
  // Fast API callbacks include the JS receiver as an implicit first argument,
  // so the generated CFunctionInfo begins with a V8Value placeholder for it.
  metadata->arg_info.emplace_back(CTypeInfo::Type::kV8Value);
  for (FastFFIType arg : args) {
    metadata->arg_info.emplace_back(ToV8Type(arg, false));
  }
  // Buffer arguments require FastApiCallbackOptions so the helper can throw on
  // invalid inputs and report the correct argument index.
  if (needs_callback_options) {
    metadata->arg_info.emplace_back(CTypeInfo::kCallbackOptionsType);
  }

  // CFunctionInfo stores pointers into `arg_info`; both are held by
  // FastFFIMetadata so they remain valid for the generated function lifetime.
  metadata->c_function_info = std::make_unique<CFunctionInfo>(
      CTypeInfo(ToV8Type(result, true)),
      metadata->arg_info.size(),
      metadata->arg_info.data(),
      needs_bigint ? CFunctionInfo::Int64Representation::kBigInt
                   : CFunctionInfo::Int64Representation::kNumber);
  metadata->c_function =
      v8::CFunction(metadata->trampoline.code, metadata->c_function_info.get());
  return metadata;
}

}  // namespace node::ffi

#endif  // HAVE_FFI
