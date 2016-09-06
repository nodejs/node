// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_CONFIGURATION_H_
#define V8_IC_HANDLER_CONFIGURATION_H_

#include "src/elements-kind.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

enum LoadHandlerType {
  kLoadICHandlerForElements = 0,
  kLoadICHandlerForProperties = 1
};

class LoadHandlerTypeBit : public BitField<bool, 0, 1> {};

// Encoding for configuration Smis for property loads:
class FieldOffsetIsInobject
    : public BitField<bool, LoadHandlerTypeBit::kNext, 1> {};
class FieldOffsetIsDouble
    : public BitField<bool, FieldOffsetIsInobject::kNext, 1> {};
class FieldOffsetOffset : public BitField<int, FieldOffsetIsDouble::kNext, 27> {
};
// Make sure we don't overflow into the sign bit.
STATIC_ASSERT(FieldOffsetOffset::kNext <= kSmiValueSize - 1);

// Encoding for configuration Smis for elements loads:
class KeyedLoadIsJsArray : public BitField<bool, LoadHandlerTypeBit::kNext, 1> {
};
class KeyedLoadConvertHole
    : public BitField<bool, KeyedLoadIsJsArray::kNext, 1> {};
class KeyedLoadElementsKind
    : public BitField<ElementsKind, KeyedLoadConvertHole::kNext, 8> {};
// Make sure we don't overflow into the sign bit.
STATIC_ASSERT(KeyedLoadElementsKind::kNext <= kSmiValueSize - 1);

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_HANDLER_CONFIGURATION_H_
