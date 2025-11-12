// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

(function() {
  function foo() {
    var a = new Array();
    a[0] = 10;
    return a;
  }

  assertEquals(1, foo().length);

  gc();
  gc();
  gc();
  gc();

  // Change prototype elements from fast smi to slow elements dictionary.
  // The validity cell is invalidated by the change of Array.prototype's
  // map.
  Array.prototype.__defineSetter__("0", function() {});

  assertEquals(0, foo().length);
})();


(function() {
  function foo() {
    var a = new Array();
    a[0] = 10;
    return a;
  }

  // Change prototype elements from fast smi to dictionary.
  Array.prototype[123456789] = 42;
  Array.prototype.length = 0;

  assertEquals(1, foo().length);

  gc();
  gc();
  gc();
  gc();

  // Change prototype elements from dictionary to slow elements dictionary.
  // The validity cell is invalidated by making the elements dictionary slow.
  Array.prototype.__defineSetter__("0", function() {});

  assertEquals(0, foo().length);
})();
