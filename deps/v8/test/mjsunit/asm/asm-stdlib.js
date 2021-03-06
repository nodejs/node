// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function FailProxyAsStdlib() {
  // Test that passing a proxy as "stdlib" will cause module instantiation to
  // fail while still only triggering one observable property load.
  function Module(stdlib, foreign, heap) {
    "use asm";
    var a = stdlib.Math.PI;
    function f() { return a }
    return { f:f };
  }
  var trap_was_called = 0;
  var proxy = new Proxy(this, { get:function(target, property, receiver) {
    trap_was_called++;
    if (property == "Math") return { PI:23 };
    return Reflect.get(target, property, receiver);
  }});
  var m = Module(proxy);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(1, trap_was_called);
  assertEquals(23, m.f());
})();

(function FailGetterInStdlib() {
  // Test that accessors as part of "stdlib" will cause module instantiation to
  // fail while still only triggering one observable property load.
  function Module(stdlib, foreign, heap) {
    "use asm";
    var a = new stdlib.Int8Array(heap);
    function f() { return a[0] | 0 }
    return { f:f };
  }
  var trap_was_called = 0;
  var observer = { get Int8Array() {
    trap_was_called++;
    return function() { return [ 23 ] };
  }};
  var m = Module(observer);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(1, trap_was_called);
  assertEquals(23, m.f());
})();
