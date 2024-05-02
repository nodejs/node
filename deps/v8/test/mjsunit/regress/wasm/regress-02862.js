// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --expose-gc --invoke-weak-callbacks --omit-quit --gc-interval=469 --validate-asm

function nop() {}
var __v_42 = {};
var __v_49 = {};
var __v_70 = {};
var __v_79 = {};
__v_58 = {
  instantiateModuleFromAsm: function(text, ffi, heap) {
    var __v_49 = eval('(' + text + ')');
    if (nop()) {
      throw "validate failure";
    }
    var __v_79 = __v_49();
    if (nop()) {
      throw "bad module args";
    }
  }};
__f_140 = function __f_140() {
  if (found === expected) {
    if (1 / expected) return;
  } else if ((expected !== expected) && (found !== found)) { return; };
};
__f_128 = function __f_128() { if (!__f_105()) { __f_125(__f_69(), found, name_opt); } };
__f_136 = function __f_136(code, type_opt, cause_opt) {
  var __v_42 = true;
  try {
    if (typeof code == 'function') { code(); }
    else { eval(); }
    __v_42 = false;
  } catch (e) {
    if (typeof type_opt == 'function') { __f_101(); }
    if (arguments.length >= 3) { __f_128(); }
    return;
  }
};
__f_101 = function __f_101() { if (obj instanceof type) {obj.constructor; if (typeof __v_57 == "function") {; }; } };
try {
__f_128();
__v_82.__p_750895751 = __v_82[getRandomProperty()];
} catch(e) {"Caught: " + e; }
__f_119();
gc();
__f_119(19, __f_136);
__f_119();
__f_119();
__f_136(function() {
  __v_58.instantiateModuleFromAsm(__f_128.toString()).__f_108();
});
function __f_119() {
  "use asm";
  function __f_108() {
  }
  return {__f_108: __f_108};
}
__f_119();
__f_119();
__f_119();
function __f_95() {
}
__f_119();
try {
__f_119();
__f_135();
} catch(e) {"Caught: " + e; }
__f_119();
__f_119();
__f_119();
function __f_105() {
  "use asm";
  function __f_108() {
  }
  return {__f_108: __f_108};
}
__f_119();
__f_119();
__f_119();
__f_119();
__f_119();
__f_119();
__f_119();
function __f_93(stdlib) {
  "use asm";
  var __v_70 = new stdlib.Int32Array();
__v_70[4294967295]|14 + 1 | 14;
  return {__f_108: __f_108};
}
function __f_135() {
  var __v_66 = new ArrayBuffer();
  var __v_54 = new Int32Array(__v_66);
  var module = __v_58.instantiateModuleFromAsm( __f_93.toString());
  __f_128();
}
(function () {
})();
(function () {
})();
try {
(function() {
      var __v_54 = 0x87654321;
 __v_66.__f_89();
})();
} catch(e) {; }
