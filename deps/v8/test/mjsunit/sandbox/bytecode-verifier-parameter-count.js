// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --verify-bytecode-full

function f() {
  return 1;
}

f();
const bytecodeObj = %GetBytecode(f);

bytecodeObj.parameter_count = 0;

%InstallBytecode(f, bytecodeObj);

assertUnreachable("Process should have been killed.");
