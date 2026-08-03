// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

const kMaxLocals = 50000;
const fn_template = '"use asm";\nfunction f() { LOCALS }\nreturn f;';
for (var num_locals = kMaxLocals; num_locals < kMaxLocals + 2; ++num_locals) {
  const fn_code = fn_template.replace(
      'LOCALS',
      Array(num_locals)
          .fill()
          .map((_, idx) => 'var l' + idx + ' = 0;')
          .join('\n'));
  const asm_fn = new Function(fn_code);
  const f = asm_fn();
  f();
  assertEquals(num_locals <= kMaxLocals, %IsAsmWasmCode(asm_fn));
}
