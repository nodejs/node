#if HAVE_FFI

#include "node_ffi.h"

#include <iostream>

namespace node {

namespace ffi {

// FFIType: Maps string type names to libffi types
ffi_type* FFIType::ToFFI(const std::string& type_name) {
  if (type_name == "void") return &ffi_type_void;
  if (type_name == "bool") return &ffi_type_uint8;
  if (type_name == "i8" || type_name == "int8") return &ffi_type_sint8;
  if (type_name == "u8" || type_name == "uint8") return &ffi_type_uint8;
  if (type_name == "i16" || type_name == "int16") return &ffi_type_sint16;
  if (type_name == "u16" || type_name == "uint16") return &ffi_type_uint16;
  if (type_name == "i32" || type_name == "int32") return &ffi_type_sint32;
  if (type_name == "u32" || type_name == "uint32") return &ffi_type_uint32;
  if (type_name == "i64" || type_name == "int64") return &ffi_type_sint64;
  if (type_name == "u64" || type_name == "uint64") return &ffi_type_uint64;
  if (type_name == "f32" || type_name == "float") return &ffi_type_float;
  if (type_name == "f64" || type_name == "double") return &ffi_type_double;
  if (type_name == "pointer" || type_name == "ptr" || type_name == "buffer" ||
      type_name == "function" || type_name == "usize" || type_name == "isize") {
    return &ffi_type_pointer;
  }
  return nullptr;
}

bool FFIType::IsValidType(const std::string& type_name) {
  return ToFFI(type_name) != nullptr;
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
