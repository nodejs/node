// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --test-small-max-function-context-stub-size

// Generate an eval scope with a very large closure.
// Ensure that vars are leaked out properly in this case.
source = "var x;";
for (var i = 0; i < 11; i++) {
  source += "  let a_" + i + " = 0;\n";
}
source += "  (function () {\n"
for (var i = 0; i < 11; i++) {
  source += "a_" + i + "++;\n";
}
source += "})();\n"

eval(source);
assertEquals(undefined, x);
