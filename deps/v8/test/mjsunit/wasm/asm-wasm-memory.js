// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

var stdlib = this;
let kMinHeapSize = 4096;

function assertValidAsm(func) {
  assertTrue(%IsAsmWasmCode(func), "must be valid asm code");
}

function assertWasm(expected, func, ffi) {
  print("Testing " + func.name + "...");
  assertEquals(
      expected, func(stdlib, ffi, new ArrayBuffer(kMinHeapSize)).caller());
  assertValidAsm(func);
}


function TestInt32HeapAccess(stdlib, foreign, buffer) {
  "use asm";

  var m = new stdlib.Int32Array(buffer);
  function caller() {
    var i = 4;

    m[0] = (i + 1) | 0;
    m[i >> 2] = ((m[0]|0) + 1) | 0;
    m[2] = ((m[i >> 2]|0) + 1) | 0;
    return m[2] | 0;
  }

  return {caller: caller};
}

assertWasm(7, TestInt32HeapAccess);


function TestInt32HeapAccessExternal() {
  var memory = new ArrayBuffer(kMinHeapSize);
  var memory_int32 = new Int32Array(memory);
  var module_decl = eval('(' + TestInt32HeapAccess.toString() + ')');
  var module = module_decl(stdlib, null, memory);
  assertValidAsm(module_decl);
  assertEquals(7, module.caller());
  assertEquals(7, memory_int32[2]);
}

TestInt32HeapAccessExternal();


function TestHeapAccessIntTypes() {
  var types = [
    [Int8Array, 'Int8Array', '>> 0'],
    [Uint8Array, 'Uint8Array', '>> 0'],
    [Int16Array, 'Int16Array', '>> 1'],
    [Uint16Array, 'Uint16Array', '>> 1'],
    [Int32Array, 'Int32Array', '>> 2'],
    [Uint32Array, 'Uint32Array', '>> 2'],
  ];
  for (var i = 0; i < types.length; i++) {
    var code = TestInt32HeapAccess.toString();
    code = code.replace('Int32Array', types[i][1]);
    code = code.replace(/>> 2/g, types[i][2]);
    var memory = new ArrayBuffer(kMinHeapSize);
    var memory_view = new types[i][0](memory);
    var module_decl = eval('(' + code + ')');
    var module = module_decl(stdlib, null, memory);
    assertValidAsm(module_decl);
    assertEquals(7, module.caller());
    assertEquals(7, memory_view[2]);
    assertValidAsm(module_decl);
  }
}

TestHeapAccessIntTypes();


function TestFloatHeapAccess(stdlib, foreign, buffer) {
  "use asm";

  var f32 = new stdlib.Float32Array(buffer);
  var f64 = new stdlib.Float64Array(buffer);
  var fround = stdlib.Math.fround;
  function caller() {
    var i = 8;
    var j = 8;
    var v = 6.0;

    f64[2] = v + 1.0;
    f64[i >> 3] = +f64[2] + 1.0;
    f64[j >> 3] = +f64[j >> 3] + 1.0;
    i = +f64[i >> 3] == 9.0;
    return i|0;
  }

  return {caller: caller};
}

assertWasm(1, TestFloatHeapAccess);


function TestFloatHeapAccessExternal() {
  var memory = new ArrayBuffer(kMinHeapSize);
  var memory_float64 = new Float64Array(memory);
  var module_decl = eval('(' + TestFloatHeapAccess.toString() + ')');
  var module = module_decl(stdlib, null, memory);
  assertValidAsm(module_decl);
  assertEquals(1, module.caller());
  assertEquals(9.0, memory_float64[1]);
}

TestFloatHeapAccessExternal();


(function() {
  function TestByteHeapAccessCompat(stdlib, foreign, buffer) {
    "use asm";

    var HEAP8 = new stdlib.Uint8Array(buffer);
    var HEAP32 = new stdlib.Int32Array(buffer);

    function store(i, v) {
      i = i | 0;
      v = v | 0;
      HEAP32[i >> 2] = v;
    }

    function storeb(i, v) {
      i = i | 0;
      v = v | 0;
      HEAP8[i | 0] = v;
    }

    function load(i) {
      i = i | 0;
      return HEAP8[i] | 0;
    }

    function iload(i) {
      i = i | 0;
      return HEAP8[HEAP32[i >> 2] | 0] | 0;
    }

    return {load: load, iload: iload, store: store, storeb: storeb};
  }

  var memory = new ArrayBuffer(kMinHeapSize);
  var module_decl = eval('(' + TestByteHeapAccessCompat.toString() + ')');
  var m = module_decl(stdlib, null, memory);
  assertValidAsm(module_decl);
  m.store(0, 20);
  m.store(4, 21);
  m.store(8, 22);
  m.storeb(20, 123);
  m.storeb(21, 42);
  m.storeb(22, 77);
  assertEquals(123, m.load(20));
  assertEquals(42, m.load(21));
  assertEquals(77, m.load(22));
  assertEquals(123, m.iload(0));
  assertEquals(42, m.iload(4));
  assertEquals(77, m.iload(8));
})();


function TestIntishAssignment(stdlib, foreign, heap) {
  "use asm";
  var HEAP32 = new stdlib.Int32Array(heap);
  function func() {
    var a = 1;
    var b = 2;
    HEAP32[0] = a + b;
    return HEAP32[0] | 0;
  }
  return {caller: func};
}

assertWasm(3, TestIntishAssignment);


function TestFloatishAssignment(stdlib, foreign, heap) {
  "use asm";
  var HEAPF32 = new stdlib.Float32Array(heap);
  var fround = stdlib.Math.fround;
  function func() {
    var a = fround(1.0);
    var b = fround(2.0);
    HEAPF32[0] = a + b;
    return +HEAPF32[0];
  }
  return {caller: func};
}

assertWasm(3, TestFloatishAssignment);


function TestDoubleToFloatAssignment(stdlib, foreign, heap) {
  "use asm";
  var HEAPF32 = new stdlib.Float32Array(heap);
  var fround = stdlib.Math.fround;
  function func() {
    var a = 1.23;
    HEAPF32[0] = a;
    return +HEAPF32[0];
  }
  return {caller: func};
}

assertWasm(Math.fround(1.23), TestDoubleToFloatAssignment);
