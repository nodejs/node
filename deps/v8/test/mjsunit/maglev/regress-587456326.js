// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --expose-externalize-string
// Flags: --expose-gc

const obj = {aaaaaaaaaaaaa: 42};

function f(s, cond) {
  // The key check compares string contents, so strings other than the
  // internalized key pass it.
  obj[s];
  // Both operands are long enough for the result to be a ConsString.
  const t = cond ? 'bbbbbbbbbbbbb' : 'ccccccccccccc';
  return s + t;
}

%PrepareFunctionForOptimization(f);
f('aaaaaaaaaaaaa', false);
f('aaaaaaaaaaaaa', true);
%OptimizeMaglevOnNextCall(f);
f('aaaaaaaaaaaaa', false);

const two_byte = createExternalizableTwoByteString('aaaaaaaaaaaaa');
assertFalse(isOneByteString(two_byte));
const cons = f(two_byte, false);
// The encoding of a ConsString must match its parts: rebuilding it from them
// must not change it.
assertEquals(
    isOneByteString(cons), isOneByteString(createExternalizableString(cons)));
// Let the heap verifier see the ConsString before the comparison below
// flattens it.
gc();
assertEquals('aaaaaaaaaaaaaccccccccccccc', cons);
