// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/d8.h"

namespace v8 {

void Shell::AddOSMethods(Isolate* isolate, Local<ObjectTemplate> os_templ) {}

char* Shell::ReadCharsFromTcpPort(const char* name, int* size_out) {
  // TODO(leszeks): No reason this shouldn't exist on windows.
  return nullptr;
}

}  // namespace v8
