// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var f = function() { return 1; };

(function func1() {
  eval("var f = function canary(s) { return 2; }");
})();

assertEquals(f(), 1);
