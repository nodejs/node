// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let __callGC;
(function () {
  __callGC = function() {};
})();

var __v_5 = {};
var __v_9 = {x: {}};

function main() {
  function __f_6() { this.elms = new Array(); }
  __f_6.prototype.size = function () { return this.elms.length; };
  function __f_7() { this.v = new __f_6(); }
  __f_7.prototype.add = function (__v_25) { this.v.elms.push(__v_25); };
  __f_7.prototype.size = function () { return this.v.size(); };
  __f_7.prototype.execute = function () {
    for (var __v_28 = 0; __v_28 < this.size(); __v_28++) {
      delete __v_9[__v_9, 538276];
      __callGC();
    }
  };
  var __v_22 = new __f_7();
  for (var __v_23 = 0; __v_23 < 10; __v_23++) {
    try {
      if (__v_5 != null && typeof __v_5 == "object") {
        try {
          Object.defineProperty(__v_5, 807285, {get: function() {}});
        } catch (e) {}
      }
      __v_22.add();
    } catch (e) {}
  }
  __v_22.execute();
}

%PrepareFunctionForOptimization(main);
main();
main();
%OptimizeFunctionOnNextCall(main);
main();
