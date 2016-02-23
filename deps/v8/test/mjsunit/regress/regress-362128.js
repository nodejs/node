// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

function genM() {
  "use strict";
  return function () {
    return this.field;
  };
}

function genR() {
  var x = {
    field: 10
  }
  return x;
}

method = {};
receiver = {};

method = genM("A");
receiver = genR("A");

var foo = (function () {
  return function suspect (name) {
    "use strict";
    return method.apply(receiver, arguments);
  }
})();

foo("a", "b", "c");
foo("a", "b", "c");
foo("a", "b", "c");
%OptimizeFunctionOnNextCall(foo);
foo("a", "b", "c");
