// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const kMax = %StringMaxLength();

// 1. FuncNameInferrer at exact String::kMaxLength boundary and overflow.
// Prefix ("A" + a's) + ".o" (2) + 10 * ".<computed>" (110) = prefix.length + 112.
// Wrapper around prefix is 13 + 93 = 106 source chars (< 112), keeping the
// eval source string strictly within String::kMaxLength.
{
  const accesses = '[i]'.repeat(10);
  const exactPrefix = 'A' + 'a'.repeat(kMax - 113);
  const exactOuter = eval(
      '(0, function ' + exactPrefix +
      '() { let i = 0; const o = {}; o[i] = o; return o' + accesses +
      ' = () => 42; })');
  const exactInner = exactOuter();
  assertEquals(42, exactInner());
  assertEquals(kMax, %FunctionGetInferredName(exactInner).length);

  const overPrefix = 'A' + 'a'.repeat(kMax - 112);
  const overOuter = eval(
      '(0, function ' + overPrefix +
      '() { let i = 0; const o = {}; o[i] = o; return o' + accesses +
      ' = () => 42; })');
  const overInner = overOuter();
  assertEquals(42, overInner());
  assertEquals('', %FunctionGetInferredName(overInner));
}

// 2. Hex BigInt property names around the rounded-down threshold
// kMaxBigIntHexSymbolLength = 2 + floor((kMax - 4) * 4 / 5) (active in
// lower_limits_mode and 32-bit builds).
const kMaxBigIntHexDigits = (1 << 28) - 16;
const maxHexDigits = Math.floor(((kMax - 4) * 4) / 5);
if (maxHexDigits + 1 <= kMaxBigIntHexDigits) {
  // Keep each eval snippet to a single key so the JS source string stays well
  // below kMax (~838 KB < 1 MB in lower_limits_mode).
  const okHex = '0x' + 'f'.repeat(maxHexDigits) + 'n';
  const getObj = eval('({ get ' + okHex + '() { return 7; } })');
  const okKey = Object.getOwnPropertyNames(getObj)[0];
  assertTrue(okKey.length <= kMax - 4);
  const getDesc = Object.getOwnPropertyDescriptor(getObj, okKey);
  assertEquals(4 + okKey.length, getDesc.get.name.length);
  assertEquals(7, getDesc.get());

  const setObj = eval('({ set ' + okHex + '(v) {} })');
  const setDesc = Object.getOwnPropertyDescriptor(setObj, okKey);
  assertEquals(4 + okKey.length, setDesc.set.name.length);

  const dataObj = eval('({ ' + okHex + ': 9 })');
  assertEquals(9, dataObj[okKey]);
  const methodObj = eval('({ ' + okHex + '() { return 8; } })');
  assertEquals(okKey.length, methodObj[okKey].name.length);
  assertEquals(8, methodObj[okKey]());
  eval('function validLazyGet() { return ({ get ' + okHex + '() {} }); }');

  // Non-hex BigInt property names do not expand and remain allowed past
  // maxHexDigits.
  const decBig = '9'.repeat(maxHexDigits + 10) + 'n';
  const decObj = eval('({ get ' + decBig + '() { return 3; } })');
  const decKey = Object.getOwnPropertyNames(decObj)[0];
  assertEquals(maxHexDigits + 10, decKey.length);
  assertEquals(3, decObj[decKey]);
  eval('function validLazyDec() { return ({ get ' + decBig + '() {} }); }');

  const octBig = '0o' + '7'.repeat(maxHexDigits + 10) + 'n';
  const octObj = eval('({ get ' + octBig + '() { return 4; } })');
  const octKey = Object.getOwnPropertyNames(octObj)[0];
  assertEquals(4, octObj[octKey]);
  eval('function validLazyOct() { return ({ get ' + octBig + '() {} }); }');

  // Hex BigInt property names exceeding kMaxBigIntHexSymbolLength are rejected
  // consistently by both Parser and PreParser.
  const overHex = '0x' + 'f'.repeat(maxHexDigits + 1) + 'n';
  const upperOverHex = '0X' + 'f'.repeat(maxHexDigits + 1) + 'n';
  for (const snippet of [
    '({ ' + overHex + ': 1 })',
    '({ get ' + overHex + '() {} })',
    '({ get ' + overHex + '() {}, a: 1 })',
    '({ set ' + overHex + '(v) {} })',
    '(class { ' + overHex + ' = 1; })',
    '(class { get ' + overHex + '() {} })',
    'function lazyOver() { return ({ ' + overHex + ': 1 }); }',
    'function lazyUpperOver() { return ({ ' + upperOverHex + ': 1 }); }',
    'function lazyGet() { return ({ get ' + overHex + '() {} }); }',
    'function lazySet() { return ({ set ' + overHex + '(v) {} }); }',
    'function lazyDestruct({ ' + overHex + ': x }) {}',
    'class C { m() { this.#x; } get ' + overHex + '() {} #x; }',
    'class C { m() { this.#x; return ({ ' + overHex + ': 1 }); } #x; }',
  ]) {
    assertThrows(() => eval(snippet), SyntaxError, /Invalid string length/);
  }
  // In destructuring cover grammar with an earlier default (`a = 1`), the outer
  // cover-grammar error may overwrite the message, while still throwing
  // SyntaxError (and triggering FatalNoSecurityImpact under
  // --correctness-fuzzer-suppressions).
  assertThrows(
      () => eval('let x; ({ a = 1, ' + overHex + ': x } = {});'), SyntaxError);
}
