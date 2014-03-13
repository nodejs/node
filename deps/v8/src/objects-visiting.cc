// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "ic-inl.h"
#include "objects-visiting.h"

namespace v8 {
namespace internal {


static inline bool IsShortcutCandidate(int type) {
  return ((type & kShortcutTypeMask) == kShortcutTypeTag);
}


StaticVisitorBase::VisitorId StaticVisitorBase::GetVisitorId(
    int instance_type,
    int instance_size) {
  if (instance_type < FIRST_NONSTRING_TYPE) {
    switch (instance_type & kStringRepresentationMask) {
      case kSeqStringTag:
        if ((instance_type & kStringEncodingMask) == kOneByteStringTag) {
          return kVisitSeqOneByteString;
        } else {
          return kVisitSeqTwoByteString;
        }

      case kConsStringTag:
        if (IsShortcutCandidate(instance_type)) {
          return kVisitShortcutCandidate;
        } else {
          return kVisitConsString;
        }

      case kSlicedStringTag:
        return kVisitSlicedString;

      case kExternalStringTag:
        return GetVisitorIdForSize(kVisitDataObject,
                                   kVisitDataObjectGeneric,
                                   instance_size);
    }
    UNREACHABLE();
  }

  switch (instance_type) {
    case BYTE_ARRAY_TYPE:
      return kVisitByteArray;

    case FREE_SPACE_TYPE:
      return kVisitFreeSpace;

    case FIXED_ARRAY_TYPE:
      return kVisitFixedArray;

    case FIXED_DOUBLE_ARRAY_TYPE:
      return kVisitFixedDoubleArray;

    case CONSTANT_POOL_ARRAY_TYPE:
      return kVisitConstantPoolArray;

    case ODDBALL_TYPE:
      return kVisitOddball;

    case MAP_TYPE:
      return kVisitMap;

    case CODE_TYPE:
      return kVisitCode;

    case CELL_TYPE:
      return kVisitCell;

    case PROPERTY_CELL_TYPE:
      return kVisitPropertyCell;

    case JS_SET_TYPE:
      return GetVisitorIdForSize(kVisitStruct,
                                 kVisitStructGeneric,
                                 JSSet::kSize);

    case JS_MAP_TYPE:
      return GetVisitorIdForSize(kVisitStruct,
                                 kVisitStructGeneric,
                                 JSMap::kSize);

    case JS_WEAK_MAP_TYPE:
      return kVisitJSWeakMap;

    case JS_WEAK_SET_TYPE:
      return kVisitJSWeakSet;

    case JS_REGEXP_TYPE:
      return kVisitJSRegExp;

    case SHARED_FUNCTION_INFO_TYPE:
      return kVisitSharedFunctionInfo;

    case JS_PROXY_TYPE:
      return GetVisitorIdForSize(kVisitStruct,
                                 kVisitStructGeneric,
                                 JSProxy::kSize);

    case JS_FUNCTION_PROXY_TYPE:
      return GetVisitorIdForSize(kVisitStruct,
                                 kVisitStructGeneric,
                                 JSFunctionProxy::kSize);

    case FOREIGN_TYPE:
      return GetVisitorIdForSize(kVisitDataObject,
                                 kVisitDataObjectGeneric,
                                 Foreign::kSize);

    case SYMBOL_TYPE:
      return kVisitSymbol;

    case FILLER_TYPE:
      return kVisitDataObjectGeneric;

    case JS_ARRAY_BUFFER_TYPE:
      return kVisitJSArrayBuffer;

    case JS_TYPED_ARRAY_TYPE:
      return kVisitJSTypedArray;

    case JS_DATA_VIEW_TYPE:
      return kVisitJSDataView;

    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_TYPE:
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_BUILTINS_OBJECT_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
      return GetVisitorIdForSize(kVisitJSObject,
                                 kVisitJSObjectGeneric,
                                 instance_size);

    case JS_FUNCTION_TYPE:
      return kVisitJSFunction;

    case HEAP_NUMBER_TYPE:
#define EXTERNAL_ARRAY_CASE(Type, type, TYPE, ctype, size)                     \
    case EXTERNAL_##TYPE##_ARRAY_TYPE:

    TYPED_ARRAYS(EXTERNAL_ARRAY_CASE)
      return GetVisitorIdForSize(kVisitDataObject,
                                 kVisitDataObjectGeneric,
                                 instance_size);
#undef EXTERNAL_ARRAY_CASE

    case FIXED_UINT8_ARRAY_TYPE:
    case FIXED_INT8_ARRAY_TYPE:
    case FIXED_UINT16_ARRAY_TYPE:
    case FIXED_INT16_ARRAY_TYPE:
    case FIXED_UINT32_ARRAY_TYPE:
    case FIXED_INT32_ARRAY_TYPE:
    case FIXED_FLOAT32_ARRAY_TYPE:
    case FIXED_UINT8_CLAMPED_ARRAY_TYPE:
      return kVisitFixedTypedArray;

    case FIXED_FLOAT64_ARRAY_TYPE:
      return kVisitFixedFloat64Array;

#define MAKE_STRUCT_CASE(NAME, Name, name) \
        case NAME##_TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
          if (instance_type == ALLOCATION_SITE_TYPE) {
            return kVisitAllocationSite;
          }

          return GetVisitorIdForSize(kVisitStruct,
                                     kVisitStructGeneric,
                                     instance_size);

    default:
      UNREACHABLE();
      return kVisitorIdCount;
  }
}

} }  // namespace v8::internal
