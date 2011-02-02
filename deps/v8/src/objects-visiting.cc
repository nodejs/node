// Copyright 2009 the V8 project authors. All rights reserved.
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
        if ((instance_type & kStringEncodingMask) == kAsciiStringTag) {
          return kVisitSeqAsciiString;
        } else {
          return kVisitSeqTwoByteString;
        }

      case kConsStringTag:
        if (IsShortcutCandidate(instance_type)) {
          return kVisitShortcutCandidate;
        } else {
          return kVisitConsString;
        }

      case kExternalStringTag:
        return GetVisitorIdForSize(kVisitDataObject,
                                   kVisitDataObjectGeneric,
                                   ExternalString::kSize);
    }
    UNREACHABLE();
  }

  switch (instance_type) {
    case BYTE_ARRAY_TYPE:
      return kVisitByteArray;

    case FIXED_ARRAY_TYPE:
      return kVisitFixedArray;

    case ODDBALL_TYPE:
      return kVisitOddball;

    case MAP_TYPE:
      return kVisitMap;

    case CODE_TYPE:
      return kVisitCode;

    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      return kVisitPropertyCell;

    case SHARED_FUNCTION_INFO_TYPE:
      return kVisitSharedFunctionInfo;

    case PROXY_TYPE:
      return GetVisitorIdForSize(kVisitDataObject,
                                 kVisitDataObjectGeneric,
                                 Proxy::kSize);

    case FILLER_TYPE:
      return kVisitDataObjectGeneric;

    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_VALUE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_REGEXP_TYPE:
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
    case PIXEL_ARRAY_TYPE:
    case EXTERNAL_BYTE_ARRAY_TYPE:
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
    case EXTERNAL_SHORT_ARRAY_TYPE:
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
    case EXTERNAL_INT_ARRAY_TYPE:
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      return GetVisitorIdForSize(kVisitDataObject,
                                 kVisitDataObjectGeneric,
                                 instance_size);

#define MAKE_STRUCT_CASE(NAME, Name, name) \
        case NAME##_TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
          return GetVisitorIdForSize(kVisitStruct,
                                     kVisitStructGeneric,
                                     instance_size);

    default:
      UNREACHABLE();
      return kVisitorIdCount;
  }
}

} }  // namespace v8::internal
