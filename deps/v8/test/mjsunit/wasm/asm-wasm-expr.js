// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

var selectedTest = undefined;
//selectedTest = 16;

function skip(a) {
  return selectedTest != undefined ? a != selectedTest : false;
}

const assign_in_stmt = [
  "if (E) =",
  "if (=) E",
  "if (E) E; else =",
  "for (=; E; S) S",
  "for (E; =; S) S",
  "for (E; E; =) E",
  "for (E; E; E) =",
  "do { = } while(E)",
  "do { S } while (=)",
];
const assign_in_expr = [
  "i32_func(=) | 0",
  "(=) ? E : E",
  "E ? (=) : E",
  "E ? E : (=)",
  "(=) + E",
  "E + (=)",
  "imul(=, E)",
  "imul(E, =)",
  "~(=)",
  "(=) | 0",
  "(=), E",
  "E, (=)",
  "E, E, (=)",
  "E, (=), E",
  "(=), E, E",
];

const stdlib = {
  Math: Math,
  Int8Array: Int8Array,
  Int16Array: Int16Array,
  Int32Array: Int32Array,
  Uint8Array: Uint8Array,
  Uint16Array: Uint16Array,
  Uint32Array: Uint32Array,
  Float32Array: Float32Array,
  Float64Array: Float64Array,
};

const buffer = new ArrayBuffer(65536);

// Template for a module.
function MODULE_TEMPLATE(stdlib, foreign, buffer) {
  "use asm";
  var imul = stdlib.Math.imul;
  var fround = stdlib.Math.fround;
  var M = new stdlib.Int32Array(buffer);
  var G = 0;

  function void_func() {}
  function i32_func(a) {
    a = a | 0;
    return a | 0;
  }

  FUNC_DECL
  return {main: main};
}

// Template for main function.
{
  function main(i32, f32, f64) {
    i32 = i32 | 0;
    f32 = fround(f32);
    f64 = +f64;
    FUNC_BODY
  }
}

function RunAsmJsTest(asmfunc, expect) {
  var asm_source = asmfunc.toString();
  var nonasm_source = asm_source.replace(new RegExp("use asm"), "");

  print("Testing " + asmfunc.name + " (js)...");
  var js_module = eval("(" + nonasm_source + ")")(stdlib, {}, buffer);
  expect(js_module);

  print("Testing " + asmfunc.name + " (asm.js)...");
  var asm_module = asmfunc(stdlib, {}, buffer);
  assertTrue(%IsAsmWasmCode(asmfunc));
  expect(asm_module);
}

var test = 0;

function DoTheTests(expr, assign, stmt) {
  // ==== Expression assignment tests ========================================
  for (let e of assign_in_expr) {
    if (skip(++test)) continue;
    var orig = e;
    e = e.replace(/=/g, assign);
    e = e.replace(/E/g, expr);
    e = e.replace(/S/g, stmt);
    var str = main.toString().replace("FUNC_BODY", "return (" + e + ") | 0;");
    var asm_source = MODULE_TEMPLATE.toString().replace("FUNC_DECL", str);
    doTest(asm_source, "(" + test + ") " + e);
  }

  // ==== Statement assignment tests =========================================
  for (let e of assign_in_stmt) {
    if (skip(++test)) continue;
    var orig = e;
    e = e.replace(/=/g, assign);
    e = e.replace(/E/g, expr);
    e = e.replace(/S/g, stmt);
    var str = main.toString().replace("FUNC_BODY",  e + "; return 0;");
    var asm_source = MODULE_TEMPLATE.toString().replace("FUNC_DECL", str);
    doTest(asm_source, "(" + test + ") " + e);
  }

  function doTest(asm_source, orig) {
    var nonasm_source = asm_source.replace(new RegExp("use asm"), "");
    print("Testing JS:    " + orig);
    var js_module = eval("(" + nonasm_source + ")")(stdlib, {}, buffer);
    expect(js_module);

    print("Testing ASMJS: " + orig);
    var asmfunc = eval("(" + asm_source + ")");
    var asm_module = asmfunc(stdlib, {}, buffer);
    assertTrue(%IsAsmWasmCode(asmfunc));
    expect(asm_module);
  }

  function expect(module) { module.main(0, 0, 0); print("  ok"); return true; }
}

DoTheTests("(i32 | 0)", "i32 = 0", "void_func()");
DoTheTests("G", "G = 0", "void_func()");
DoTheTests("G", "G = 0", "G");
DoTheTests("(M[0] | 0)", "M[0] = 0", "void_func()");
