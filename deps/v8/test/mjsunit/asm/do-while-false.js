// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";

  function d0() {
    do { } while(false);
    return 110;
  }

  function d1() {
    do { return 111; } while(false);
    return 112;
  }

  function d2() {
    do { break; } while(false);
    return 113;
  }

  function d3(a) {
    a = a | 0;
    do { if (a) return 114; } while(false);
    return 115;
  }

  function d4(a) {
    a = a | 0;
    do { if (a) return 116; else break; } while(false);
    return 117;
  }

  function d5(a) {
    a = a | 0;
    do { if (a) return 118; } while(false);
    return 119;
  }

  function d6(a) {
    a = a | 0;
    do {
      if ((a | 0) == 0) return 120;
      if ((a | 0) == 1) break;
      if ((a | 0) == 2) return 122;
      if ((a | 0) == 3) continue;
      if ((a | 0) == 4) return 124;
    } while(false);
    return 125;
  }

  return {d0: d0, d1: d1, d2: d2, d3: d3, d4: d4, d5: d5, d6: d6};
}

var m = Module();

assertEquals(110, m.d0());

assertEquals(111, m.d1());

assertEquals(113, m.d2());

assertEquals(114, m.d3(1));
assertEquals(115, m.d3(0));

assertEquals(116, m.d4(1));
assertEquals(117, m.d4(0));

assertEquals(118, m.d5(1));
assertEquals(119, m.d5(0));

assertEquals(120, m.d6(0));
assertEquals(125, m.d6(1));
assertEquals(122, m.d6(2));
assertEquals(125, m.d6(3));
assertEquals(124, m.d6(4));
assertEquals(125, m.d6(5));
