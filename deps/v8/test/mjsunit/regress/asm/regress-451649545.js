// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const source = 'function Module() {\n' +
  '  "use asm";\n' +
  '  var x = 0 /* \r */ var y = 0;\n' +
  '  function f() { return 0; }\n' +
  '  return f;\n' +
  '}\n' +
  'Module();\n';

eval(source);
assertTrue(%IsAsmWasmCode(Module));
