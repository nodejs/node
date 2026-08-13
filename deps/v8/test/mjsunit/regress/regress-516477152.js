// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

%RuntimeEvaluateREPL("let x = 1;");
%RuntimeEvaluateREPL(`
  let x = 2;
  let resolved = eval("x");
  assertEquals(2, resolved);
`);
