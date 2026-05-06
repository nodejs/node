#include "ffi/fastcall/cfunction_info.h"

#ifdef HAVE_FFI_FASTCALL

#include "ffi/types.h"
#include "util.h"

namespace node::ffi::fastcall {

namespace {

// Map an ffi_type to (CTypeInfo::Type, ArgClass) for argument positions.
struct ArgMapping {
  v8::CTypeInfo::Type ctype;
  ArgClass cls;
};

// Map an ffi_type to (CTypeInfo::Type, ResultClass) for the return position.
struct ResultMapping {
  v8::CTypeInfo::Type ctype;
  ResultClass cls;
};

ArgMapping MapArgType(ffi_type* t) {
  using T = v8::CTypeInfo::Type;
  if (t == &ffi_type_sint8 ||
      t == &ffi_type_sint16 ||
      t == &ffi_type_sint32)  return {T::kInt32,   ArgClass::kGP};
  if (t == &ffi_type_uint8 ||
      t == &ffi_type_uint16)  return {T::kInt32,   ArgClass::kGP};
  if (t == &ffi_type_uint32)  return {T::kUint32,  ArgClass::kGP};
  if (t == &ffi_type_sint64)  return {T::kInt64,   ArgClass::kGP};
  if (t == &ffi_type_uint64 ||
      t == &ffi_type_pointer) return {T::kUint64,  ArgClass::kGP};
  if (t == &ffi_type_float)   return {T::kFloat32, ArgClass::kFP};
  if (t == &ffi_type_double)  return {T::kFloat64, ArgClass::kFP};
  // Unreachable if eligibility was checked before calling BuildCFunctionInfo.
  UNREACHABLE("FFI fast-call: MapArgType called with type that passed "
              "eligibility but has no arg mapping");
}

ResultMapping MapResultType(ffi_type* t) {
  using T = v8::CTypeInfo::Type;
  if (t == &ffi_type_void)    return {T::kVoid,    ResultClass::kVoid};
  if (t == &ffi_type_sint8 ||
      t == &ffi_type_sint16 ||
      t == &ffi_type_sint32)  return {T::kInt32,   ResultClass::kGP};
  if (t == &ffi_type_uint8 ||
      t == &ffi_type_uint16)  return {T::kInt32,   ResultClass::kGP};
  if (t == &ffi_type_uint32)  return {T::kUint32,  ResultClass::kGP};
  if (t == &ffi_type_sint64)  return {T::kInt64,   ResultClass::kGP};
  if (t == &ffi_type_uint64 ||
      t == &ffi_type_pointer) return {T::kUint64,  ResultClass::kGP};
  if (t == &ffi_type_float)   return {T::kFloat32, ResultClass::kFP};
  if (t == &ffi_type_double)  return {T::kFloat64, ResultClass::kFP};
  UNREACHABLE("FFI fast-call: MapResultType called with type that passed "
              "eligibility but has no result mapping");
}

}  // namespace

CFunctionInfoBundle::~CFunctionInfoBundle() {
  ::operator delete[](arg_types);
  delete info;
}

CFunctionInfoBundle::CFunctionInfoBundle(CFunctionInfoBundle&& o) noexcept
    : info(o.info),
      arg_types(o.arg_types),
      arg_classes(std::move(o.arg_classes)),
      result_class(o.result_class) {
  o.info = nullptr;
  o.arg_types = nullptr;
  o.result_class = ResultClass::kVoid;
}

CFunctionInfoBundle& CFunctionInfoBundle::operator=(
    CFunctionInfoBundle&& o) noexcept {
  if (this != &o) {
    ::operator delete[](arg_types);
    delete info;
    info = o.info;
    arg_types = o.arg_types;
    arg_classes = std::move(o.arg_classes);
    result_class = o.result_class;
    o.info = nullptr;
    o.arg_types = nullptr;
    o.result_class = ResultClass::kVoid;
  }
  return *this;
}

CFunctionInfoBundle BuildCFunctionInfo(const FFIFunction& fn) {
  static_assert(std::is_trivially_destructible_v<v8::CTypeInfo>,
                "CTypeInfo must be trivially destructible for placement-new "
                "array without explicit element destruction");
  CFunctionInfoBundle b;

  // We register the C function with HasReceiver=kNo, so V8 does not pass a
  // JS receiver in the first parameter register. The CTypeInfo[] holds only
  // user-arg types — no receiver slot. CTypeInfo has no default constructor,
  // so we allocate raw storage and placement-new each element.
  size_t n = fn.args.size();
  if (n > 0) {
    void* raw = ::operator new[](n * sizeof(v8::CTypeInfo));
    b.arg_types = static_cast<v8::CTypeInfo*>(raw);
    b.arg_classes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      auto m = MapArgType(fn.args[i]);
      new (&b.arg_types[i]) v8::CTypeInfo(m.ctype);
      b.arg_classes.push_back(m.cls);
    }
  }

  auto rm = MapResultType(fn.return_type);
  b.result_class = rm.cls;
  v8::CTypeInfo return_info(rm.ctype);

  b.info = new v8::CFunctionInfo(
      return_info,
      static_cast<unsigned int>(n),
      b.arg_types,
      v8::CFunctionInfo::Int64Representation::kBigInt,
      v8::CFunctionInfo::HasReceiver::kNo);

  return b;
}

}  // namespace node::ffi::fastcall

#endif  // HAVE_FFI_FASTCALL
