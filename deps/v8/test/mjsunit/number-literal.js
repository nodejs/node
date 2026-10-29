// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test(message, a, b, skipStrictMode) {
  assertSame(eval(a), eval(b), message);
  if (!skipStrictMode) {
    (function() {
      'use strict';
      assertSame(eval(a), eval(b), message);
    })();
  }
}

test('hex-int', '0x20', '32');
test('oct-int', '040', '32', true);  // Octals disallowed in strict mode.
test('dec-int', '32.00', '32');
test('dec-underflow-int', '32.00000000000000000000000000000000000000001', '32');
test('exp-int', '3.2e1', '32');
test('exp-int', '3200e-2', '32');
test('overflow-inf', '1e2000', 'Infinity');
test('overflow-inf-exact', '1.797693134862315808e+308', 'Infinity');
test('non-overflow-inf-exact', '1.797693134862315807e+308',
     '1.7976931348623157e+308');
test('underflow-0', '1e-2000', '0');
test('underflow-0-exact', '2.4703282292062E-324', '0');
test('non-underflow-0-exact', '2.4703282292063E-324', '5e-324');
test('precission-loss-high', '9007199254740992', '9007199254740993');
test('precission-loss-low', '1.9999999999999998', '1.9999999999999997');
test('non-canonical-literal-int', '1.0', '1');
test('non-canonical-literal-frac', '1.50', '1.5');
test('rounding-down', '1.12512512512512452', '1.1251251251251244');
test('rounding-up', '1.12512512512512453', '1.1251251251251246');
