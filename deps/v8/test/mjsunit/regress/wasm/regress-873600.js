// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function DoTest() {

  var stdlib = this;
  try {
    var buffer = new ArrayBuffer((2097120) * 1024);
  } catch (e) {
    // Out of memory: soft pass because 2GiB is actually a lot!
    print("OOM: soft pass");
    return;
  }
  var foreign = {}

  var m = (function Module(stdlib, foreign, heap) {
    "use asm";
    var MEM16 = new stdlib.Int16Array(heap);
    function load(i) {
      i = i|0;
      i = MEM16[i >> 1]|0;
      return i | 0;
    }
    function store(i, v) {
      i = i|0;
      v = v|0;
      MEM16[i >> 1] = v;
    }
    function load8(i) {
      i = i|0;
      i = MEM16[i + 8 >> 1]|0;
      return i | 0;
    }
    function store8(i, v) {
      i = i|0;
      v = v|0;
      MEM16[i + 8 >> 1] = v;
    }
    return { load: load, store: store, load8: load8, store8: store8 };
  })(stdlib, foreign, buffer);

  assertEquals(0, m.load(-8));
  assertEquals(0, m.load8(-16));
  m.store(2014, 2, 30, 1, 0);
  assertEquals(0, m.load8(-8));
  m.store8(-8, 99);
  assertEquals(99, m.load(0));
  assertEquals(99, m.load8(-8));
})();
