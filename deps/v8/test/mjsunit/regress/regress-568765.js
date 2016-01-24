// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --gc-interval=216
// Flags: --nonative-context-specialization

function PrettyPrint() { return ""; }
function fail() { }
assertSame = function assertSame() { if (found === expected) { if (1 / found) return; } else if ((expected !== expected) && (found !== found)) { return; }; }; assertEquals = function assertEquals(expected, found, name_opt) { if ( expected) { fail(PrettyPrint()); } };
assertTrue = function assertTrue() { assertEquals(); };
assertThrows = function assertThrows(code, type_opt, cause_opt) { var threwException = true; try { if (typeof code == 'function') { code(); } else {; } threwException = false; } catch (e) { if (typeof type_opt == 'function') {; } if (arguments.length >= 3) {; } return; } };
assertInstanceof = function assertInstanceof() { if (obj instanceof type) { var actualTypeName = null; var actualConstructor = Object.getPrototypeOf().constructor; if (typeof actualConstructor == "function") {; }; } };
function modifyPropertyOrValue() { var names; try {; } catch(e) {; return; } if(!names) return; name = names[rand_value % names.length]; if (isNaN()); }
function nop() {}
var __v_5 = {};
var __v_12 = {};
var __v_13 = {};
var __v_16 = {};
function __f_0() {
}
(function (){
  function __f_6() {
  }
  a = __f_6();
  b = __f_6();
  name = "Array";
})();
(function (){
  function __f_1() {
    assertTrue();
  }
  __f_1();
})();
__v_10 = {
}
__v_11 = new Object();
tailee1 = function() {
  "use strict";
  if (__v_12-- == 0) {
  }
  return nop();
};
%OptimizeFunctionOnNextCall(tailee1);
assertEquals(__v_10, tailee1.call());
__v_14 = 100000;
gc();
tailee2 = function() {
  "use strict";
  __v_14 = ((__v_14 | 0) - 1) | 0;
  if ((__v_14 | 0) === 0) {
  }
};
%OptimizeFunctionOnNextCall(tailee2);
assertEquals(__v_11, tailee2.call());
__v_13 = 999999;
tailee3 = function() {
  "use strict";
  if (__v_13-- == 0) {
  }
};
%OptimizeFunctionOnNextCall(tailee3);
assertEquals(__v_9, tailee3.call(__v_11, __v_9));
tailee4 = function(px) {
  return nop(tailee4, this, px, undefined);
};
%OptimizeFunctionOnNextCall(tailee4);
assertThrows(function() { tailee4.call(); });
tailee5 = function() {
  return nop();
};
%OptimizeFunctionOnNextCall(tailee5);
assertThrows(function() { tailee5.call(); });
tailee6 = function() {
}
tailee7 = function( py, pz, pa, pb, pc) {
  return nop();
};
%OptimizeFunctionOnNextCall(tailee7);
 tailee7.call();

(function() {
  Number.prototype[0] = "a";
  Number.prototype[1] = "b";
  Object.defineProperty(Number.prototype, 2, {
    get: function() {
    }
  });
  Number.prototype.length = 3;
Array.prototype.includes.call(5);
})();
var __v_9 = -8;
var __v_20 = 0;
