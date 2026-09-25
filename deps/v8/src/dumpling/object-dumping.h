// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DUMPLING_OBJECT_DUMPING_H_
#define V8_DUMPLING_OBJECT_DUMPING_H_

#include <iostream>

#include "src/objects/tagged.h"

namespace v8::internal {

std::string DifferentialFuzzingPrint(Tagged<Object> obj, int depth);

void DifferentialFuzzingPrint(Tagged<Object> obj, std::ostream& os);


}  // namespace v8::internal

#endif  // V8_DUMPLING_OBJECT_DUMPING_H_
