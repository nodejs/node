// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --disable-abortjs --disable-in-process-stack-traces

const count = 1048573;
const filler = "()=>0;";
const s = filler.repeat(count) + "(x = eval('')) => {}";

assertThrows(() => {
  new Function(s);
}, SyntaxError, "Too many eval calls in script");
