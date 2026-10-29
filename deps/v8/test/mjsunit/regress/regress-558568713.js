// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  let padding = " ".repeat(270000000);
  let params = [];
  for (let i = 0; i < 16384; i++) {
    params.push("p" + i);
  }
  params.push("p16384=1");
  let param_str = params.join(",");
  let inner_funcs = "";
  for (let i = 0; i < 16400; i++) {
    inner_funcs += "function f" + i + "(){}\n";
  }
  let code = "function outer() {\n  function inner(" + param_str + ") {\n" + inner_funcs + "  }\n}\n";
  eval(padding + code);
} catch (e) {
  assertInstanceof(e, RangeError);
}
