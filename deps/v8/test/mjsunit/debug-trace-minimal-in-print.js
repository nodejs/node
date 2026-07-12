// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --detailed-error-stack-trace

const obj = {
  toString() {
    %DebugTraceMinimal();
    return "test";
  }
};
print(obj);
