// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --disallow-code-generation-from-strings

assertThrows("1 + 1", EvalError);
assertThrows(() => eval("1 + 1"), EvalError);
assertThrows(() => Function("x", "return x + 1"), EvalError);
