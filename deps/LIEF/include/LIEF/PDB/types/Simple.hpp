/* Copyright 2022 - 2026 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PDB_TYPE_SIMPLE_H
#define LIEF_PDB_TYPE_SIMPLE_H

#include "LIEF/visibility.h"
#include "LIEF/PDB/Type.hpp"

namespace LIEF {
namespace pdb {
namespace types {

/// This class represents primitive types (int, float, ...) which are
/// also named *simple* types in the PDB format.
class LIEF_API Simple : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  Simple(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  /// Identifier of the primitive type.
  ///
  /// These values correspond to the low bits of the CodeView Type Index.
  enum class TYPES {
    UNKNOWN = 0,
    VOID_ = 0x0003,

    // --- Characters ---
    SCHAR = 0x0010, //!< Signed Character
    UCHAR = 0x0020, //!< Unsigned Character
    RCHAR = 0x0070, //!< "Real" Character (char)

    // --- Unicode / Wide Characters ---
    WCHAR = 0x0071,  //!< Wide Character (wchar_t)
    CHAR16 = 0x007a, //!< 16-bit Character (char16_t)
    CHAR32 = 0x007b, //!< 32-bit Character (char32_t)
    CHAR8 = 0x007c,  //!< 8-bit Character (char8_t)

    // --- Bytes ---
    SBYTE = 0x0068, //!< Signed Byte
    UBYTE = 0x0069, //!< Unsigned Byte

    // --- Short (16-bit) ---
    SSHORT = 0x0011, //!< Signed Short
    USHORT = 0x0021, //!< Unsigned Short

    SINT16 = 0x0072, //!< Explicit Signed 16-bit Integer
    UINT16 = 0x0073, //!< Explicit Unsigned 16-bit Integer

    // --- Long (32-bit) ---
    SLONG = 0x0012, //!< Signed Long
    ULONG = 0x0022, //!< Unsigned Long

    SINT32 = 0x0074, //!< Explicit Signed 32-bit Integer
    UINT32 = 0x0075, //!< Explicit Unsigned 32-bit Integer

    // --- Quad (64-bit) ---
    SQUAD = 0x0013, //!< Signed Quadword
    UQUAD = 0x0023, //!< Unsigned Quadword

    SINT64 = 0x0076, //!< Explicit Signed 64-bit Integer
    UINT64 = 0x0077, //!< Explicit Unsigned 64-bit Integer

    // --- Octa (128-bit) ---
    SOCTA = 0x0014, //!< Signed Octaword
    UOCTA = 0x0024, //!< Unsigned Octaword

    SINT128 = 0x0078, //!< Explicit Signed 128-bit Integer
    UINT128 = 0x0079, //!< Explicit Unsigned 128-bit Integer

    // --- Floating Point ---
    FLOAT16 = 0x0046, //!< 16-bit Floating point
    FLOAT32 = 0x0040, //!< 32-bit Floating point (float)
    FLOAT32_PARTIAL_PRECISION = 0x45,

    FLOAT48 = 0x0044,  //!< 48-bit Floating point
    FLOAT64 = 0x0041,  //!< 64-bit Floating point (double)
    FLOAT80 = 0x0042,  //!< 80-bit Floating point
    FLOAT128 = 0x0043, //!< 128-bit Floating point

    // --- Complex Numbers ---
    COMPLEX16 = 0x0056,
    COMPLEX32 = 0x0050,
    COMPLEX32_PARTIAL_PRECISION = 0x0055,
    COMPLEX48 = 0x0054,
    COMPLEX64 = 0x0051,
    COMPLEX80 = 0x0052,
    COMPLEX128 = 0x0053,

    // --- Booleans ---
    BOOL8 = 0x0030,   //!< 8-bit Boolean
    BOOL16 = 0x0031,  //!< 16-bit Boolean
    BOOL32 = 0x0032,  //!< 32-bit Boolean
    BOOL64 = 0x0033,  //!< 64-bit Boolean
    BOOL128 = 0x0034, //!< 128-bit Boolean
  };

  /// Modifier applied to the underlying type.
  ///
  /// In the PDB Simple Type encoding, these represent pointer attributes.
  enum class MODES : uint32_t {
    DIRECT = 0x00000000,          //!< Not a pointer (direct access)
    FAR_POINTER = 0x00000200,     //!< Far pointer
    HUGE_POINTER = 0x00000300,    //!< Huge pointer
    NEAR_POINTER32 = 0x00000400,  //!< 32-bit Near pointer
    FAR_POINTER32 = 0x00000500,   //!< 32-bit Far pointer
    NEAR_POINTER64 = 0x00000600,  //!< 64-bit Near pointer
    NEAR_POINTER128 = 0x00000700, //!< 128-bit Near pointer
  };

  /// Returns the underlying primitive type.
  TYPES type() const;

  /// Returns the mode (pointer type) of this Simple type.
  MODES modes() const;

  /// Check if this simple type is a pointer.
  bool is_pointer() const {
    return modes() != MODES::DIRECT;
  }

  /// Check if the underlying type is signed
  bool is_signed() const {
    const TYPES ty = type();
    return ty == TYPES::SCHAR || ty == TYPES::SBYTE || ty == TYPES::SSHORT ||
           ty == TYPES::SINT16 || ty == TYPES::SLONG || ty == TYPES::SINT32 ||
           ty == TYPES::SQUAD || ty == TYPES::SINT64 || ty == TYPES::SOCTA ||
           ty == TYPES::SINT128;
  }

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::SIMPLE;
  }

  ~Simple() override;
};

}
}
}
#endif
