// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Int32Array.prototype.__proto__, 'length', {
  get: function() { throw new Error('Custom length property'); }
});

var a = Math.random();

// This tests MathRandomRaw.
var v0 = new Set();
var v1 = new Object();
v0.add(v1);
