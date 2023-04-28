// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var array = [];
var v = 0;

Object.defineProperty(Array.prototype, "0", {
  get: function() { return "get " + v; },
  set: function(value) { v += value; }
});

array[0] = 10;
assertEquals(0, array.length);
assertEquals(10, v);
assertEquals("get 10", array[0]);

array.push(100);
assertEquals(1, array.length);
assertEquals(110, v);
assertEquals("get 110", array[0]);
