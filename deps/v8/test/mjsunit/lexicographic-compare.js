// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function () {

  assertFalse(%IsSmi(2147483648), 'Update test for >32 bit Smi');

  // Collect a list of interesting Smis.
  const seen = {};
  const smis = [];
  function add(x) {
    if (x | 0 == x) {
      x = x | 0;  // Canonicalizes to Smi if 32-bit signed and fits in Smi.
    }
    if (%_IsSmi(x) && !seen[x]) {
      seen[x] = 1;
      smis.push(x);
    }
  }
  function addSigned(x) {
    add(x);
    add(-x);
  }

  var BIGGER_THAN_ANY_SMI = 10 * 1000 * 1000 * 1000;
  for (var xb = 1; xb <= BIGGER_THAN_ANY_SMI; xb *= 10) {
    for (var xf = 0; xf <= 9; xf++) {
      for (var xo = -1; xo <= 1; xo++) {
        addSigned(xb * xf + xo);
      }
    }
  }

  console.log("A")

  for (var yb = 1; yb <= BIGGER_THAN_ANY_SMI; yb *= 2) {
    for (var yo = -2; yo <= 2; yo++) {
      addSigned(yb + yo);
    }
  }

  function test(x,y) {
    const lex = %SmiLexicographicCompare(x, y);
    const expected = (x == y) ? 0 : (("" + x) < ("" + y) ? -1 : 1);
    return lex == expected;
  }

  console.log(smis.length);

  for (var i = 0; i < smis.length; i++) {
    for (var j = 0; j < smis.length; j++) {
      const x = smis[i];
      const y = smis[j];
      assertTrue(test(x, y), x + " < " + y);;
    }
  }

  console.log("C")
})();
