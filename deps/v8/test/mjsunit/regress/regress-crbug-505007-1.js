// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100 --allow-natives-syntax

var count = 0;
function f() {
  try {
    f();
  } catch(e) {
    if (count < 100) {
      count++;
      %GetDebugContext();
    }
  }
}
f();
