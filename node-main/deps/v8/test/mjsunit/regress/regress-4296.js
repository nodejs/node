// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
  var o = new String("ab");
  function store(o, i, v) { o[i] = v; }
  function load(o, i) { return o[i]; }

  // Initialize the IC.
  store(o, 2, 10);
  load(o, 2);

  store(o, 0, 100);
  assertEquals("a", load(o, 0));
})();

(function () {
  var o = {__proto__: new String("ab")};
  function store(o, i, v) { o[i] = v; }
  function load(o, i) { return o[i]; }

  // Initialize the IC.
  store(o, 2, 10);
  load(o, 2);

  store(o, 0, 100);
  assertEquals("a", load(o, 0));
})();

(function () {
  "use strict";
  var o = {__proto__: {}};
  function store(o, i, v) { o[i] = v; }

  // Initialize the IC.
  store(o, 0, 100);
  o.__proto__.__proto__ = new String("bla");
  assertThrows(function () { store(o, 1, 100) });
})();
