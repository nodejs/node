// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  obj = Object();
  obj.x = Math.floor();
  return obj.x;
}

%EnsureFeedbackVectorForFunction(foo);
assertNotEquals(undefined, foo());
assertNotEquals(undefined, foo());
