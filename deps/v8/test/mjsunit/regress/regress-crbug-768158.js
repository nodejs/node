// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --always-opt

(function testOriginalRepro() {
  var result;
  var dict = { toString() { result = v;} };
  for (var v of ['fontsize', 'sup']) {
    String.prototype[v].call(dict);
    assertEquals(v, result);
  }
})();

(function testSimpler() {
  var result;
  function setResult() { result = v; }
  for (var v of ['hello', 'world']) {
    setResult();
    assertEquals(v, result);
  }
})();
