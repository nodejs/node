// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

%EnableCodeLoggingForTesting();

function module() {
  "use asm";
  function f() {
    var i = 4;
    return i | 0;
  }
  return {f: f};
}

module().f();
