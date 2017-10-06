// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_TEST_INTERFACE_H_
#define V8_INSPECTOR_TEST_INTERFACE_H_

#include "include/v8.h"

namespace v8_inspector {

class V8Inspector;

V8_EXPORT void SetMaxAsyncTaskStacksForTest(V8Inspector* inspector, int limit);
V8_EXPORT void DumpAsyncTaskStacksStateForTest(V8Inspector* inspector);

}  //  v8_inspector

#endif  //  V8_INSPECTOR_TEST_INTERFACE_H_
