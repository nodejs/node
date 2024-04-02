// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_VISITORS_INL_H_
#define V8_OBJECTS_VISITORS_INL_H_

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

ObjectVisitorWithCageBases::ObjectVisitorWithCageBases(
    PtrComprCageBase cage_base, PtrComprCageBase code_cage_base)
#if V8_COMPRESS_POINTERS
    : cage_base_(cage_base)
#ifdef V8_EXTERNAL_CODE_SPACE
      ,
      code_cage_base_(code_cage_base)
#endif  // V8_EXTERNAL_CODE_SPACE
#endif  // V8_COMPRESS_POINTERS
{
}

ObjectVisitorWithCageBases::ObjectVisitorWithCageBases(Isolate* isolate)
#if V8_COMPRESS_POINTERS
    : ObjectVisitorWithCageBases(PtrComprCageBase(isolate->cage_base()),
                                 PtrComprCageBase(isolate->code_cage_base()))
#else
    : ObjectVisitorWithCageBases(PtrComprCageBase(), PtrComprCageBase())
#endif  // V8_COMPRESS_POINTERS
{
}

ObjectVisitorWithCageBases::ObjectVisitorWithCageBases(Heap* heap)
    : ObjectVisitorWithCageBases(Isolate::FromHeap(heap)) {}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_VISITORS_INL_H_
