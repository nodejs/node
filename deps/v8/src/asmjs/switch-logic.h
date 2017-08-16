// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_SWITCH_LOGIC_H
#define V8_ASMJS_SWITCH_LOGIC_H

#include "src/globals.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

struct CaseNode : public ZoneObject {
  const int begin;
  const int end;
  CaseNode* left;
  CaseNode* right;
  CaseNode(int begin, int end) : begin(begin), end(end) {
    left = nullptr;
    right = nullptr;
  }
};

V8_EXPORT_PRIVATE CaseNode* OrderCases(ZoneVector<int>* cases, Zone* zone);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_ASMJS_SWITCH_LOGIC_H
