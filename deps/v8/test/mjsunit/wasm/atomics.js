// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-threads

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

const kMemtypeSize32 = 4;
const kMemtypeSize16 = 2;
const kMemtypeSize8 = 1;

function Add(a, b) { return a + b; }
function Sub(a, b) { return a - b; }
function And(a, b) { return a & b; }
function Or(a, b) { return a | b; }
function Xor(a, b) { return a ^ b; }
function Exchange(a, b) { return b; }

let maxSize = 10;
let memory = new WebAssembly.Memory({initial: 1, maximum: maxSize, shared: true});

function GetAtomicBinOpFunction(wasmExpression) {
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "imported_mem");
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kAtomicPrefix,
      wasmExpression])
    .exportAs("main");

  // Instantiate module, get function exports
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = (new WebAssembly.Instance(module,
        {m: {imported_mem: memory}}));
  return instance.exports.main;
}

function GetAtomicCmpExchangeFunction(wasmExpression) {
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "imported_mem");
  builder.addFunction("main", kSig_i_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kAtomicPrefix,
      wasmExpression])
    .exportAs("main");

  // Instantiate module, get function exports
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = (new WebAssembly.Instance(module,
        {m: {imported_mem: memory}}));
  return instance.exports.main;
}


function VerifyBoundsCheck(func, memtype_size) {
  const kPageSize = 65536;
  // Test out of bounds at boundary
  for (let i = memory.buffer.byteLength - memtype_size + 1;
       i < memory.buffer.byteLength + memtype_size + 4; i++) {
    assertTraps(kTrapMemOutOfBounds, () => func(i, 5, 10));
  }
  // Test out of bounds at maximum + 1
  assertTraps(kTrapMemOutOfBounds, () => func((maxSize + 1) * kPageSize, 5, 1));
}

function Test32Op(operation, func) {
  let i32 = new Uint32Array(memory.buffer);
  for (let i = 0; i < i32.length; i++) {
    let expected = 0x9cedf00d;
    let value = 0x11111111;
    i32[i] = expected;
    assertEquals(expected, func(i * kMemtypeSize32, value) >>> 0);
    assertEquals(operation(expected, value) >>> 0, i32[i]);
  }
  VerifyBoundsCheck(func, kMemtypeSize32);
}

function Test16Op(operation, func) {
  let i16 = new Uint16Array(memory.buffer);
  for (let i = 0; i < i16.length; i++) {
    let expected = 0xd00d;
    let value = 0x1111;
    i16[i] = expected;
    assertEquals(expected, func(i * kMemtypeSize16, value));
    assertEquals(operation(expected, value), i16[i]);
  }
  VerifyBoundsCheck(func, kMemtypeSize16);
}

function Test8Op(operation, func) {
  let i8 = new Uint8Array(memory.buffer);
  for (let i = 0; i < i8.length; i++) {
    let expected = 0xbe;
    let value = 0x12;
    i8[i] = expected;
    assertEquals(expected, func(i * kMemtypeSize8, value));
    assertEquals(operation(expected, value), i8[i]);
  }
  VerifyBoundsCheck(func, kMemtypeSize8, 10);
}

(function TestAtomicAdd() {
  print("TestAtomicAdd");
  let wasmAdd = GetAtomicBinOpFunction(kExprI32AtomicAdd);
  Test32Op(Add, wasmAdd);
})();

(function TestAtomicAdd16U() {
  print("TestAtomicAdd16U");
  let wasmAdd = GetAtomicBinOpFunction(kExprI32AtomicAdd16U);
  Test16Op(Add, wasmAdd);
})();

(function TestAtomicAdd8U() {
  print("TestAtomicAdd8U");
  let wasmAdd = GetAtomicBinOpFunction(kExprI32AtomicAdd8U);
  Test8Op(Add, wasmAdd);
})();

(function TestAtomicSub() {
  print("TestAtomicSub");
  let wasmSub = GetAtomicBinOpFunction(kExprI32AtomicSub);
  Test32Op(Sub, wasmSub);
})();

(function TestAtomicSub16U() {
  print("TestAtomicSub16U");
  let wasmSub = GetAtomicBinOpFunction(kExprI32AtomicSub16U);
  Test16Op(Sub, wasmSub);
})();

(function TestAtomicSub8U() {
  print("TestAtomicSub8U");
  let wasmSub = GetAtomicBinOpFunction(kExprI32AtomicSub8U);
  Test8Op(Sub, wasmSub);
})();

(function TestAtomicAnd() {
  print("TestAtomicAnd");
  let wasmAnd = GetAtomicBinOpFunction(kExprI32AtomicAnd);
  Test32Op(And, wasmAnd);
})();

(function TestAtomicAnd16U() {
  print("TestAtomicAnd16U");
  let wasmAnd = GetAtomicBinOpFunction(kExprI32AtomicAnd16U);
  Test16Op(And, wasmAnd);
})();

(function TestAtomicAnd8U() {
  print("TestAtomicAnd8U");
  let wasmAnd = GetAtomicBinOpFunction(kExprI32AtomicAnd8U);
  Test8Op(And, wasmAnd);
})();

(function TestAtomicOr() {
  print("TestAtomicOr");
  let wasmOr = GetAtomicBinOpFunction(kExprI32AtomicOr);
  Test32Op(Or, wasmOr);
})();

(function TestAtomicOr16U() {
  print("TestAtomicOr16U");
  let wasmOr = GetAtomicBinOpFunction(kExprI32AtomicOr16U);
  Test16Op(Or, wasmOr);
})();

(function TestAtomicOr8U() {
  print("TestAtomicOr8U");
  let wasmOr = GetAtomicBinOpFunction(kExprI32AtomicOr8U);
  Test8Op(Or, wasmOr);
})();

(function TestAtomicXor() {
  print("TestAtomicXor");
  let wasmXor = GetAtomicBinOpFunction(kExprI32AtomicXor);
  Test32Op(Xor, wasmXor);
})();

(function TestAtomicXor16U() {
  print("TestAtomicXor16U");
  let wasmXor = GetAtomicBinOpFunction(kExprI32AtomicXor16U);
  Test16Op(Xor, wasmXor);
})();

(function TestAtomicXor8U() {
  print("TestAtomicXor8U");
  let wasmXor = GetAtomicBinOpFunction(kExprI32AtomicXor8U);
  Test8Op(Xor, wasmXor);
})();

(function TestAtomicExchange() {
  print("TestAtomicExchange");
  let wasmExchange = GetAtomicBinOpFunction(kExprI32AtomicExchange);
  Test32Op(Exchange, wasmExchange);
})();

(function TestAtomicExchange16U() {
  print("TestAtomicExchange16U");
  let wasmExchange = GetAtomicBinOpFunction(kExprI32AtomicExchange16U);
  Test16Op(Exchange, wasmExchange);
})();

(function TestAtomicExchange8U() {
  print("TestAtomicExchange8U");
  let wasmExchange = GetAtomicBinOpFunction(kExprI32AtomicExchange8U);
  Test8Op(Exchange, wasmExchange);
})();

function TestCmpExchange(func, buffer, params, size) {
  for (let i = 0; i < buffer.length; i++) {
    for (let j = 0; j < params.length; j++) {
      for (let k = 0; k < params.length; k++) {
        buffer[i] = params[j];
        let loaded = func(i * size, params[k], params[j]) >>> 0;
        let expected = (params[k] == loaded) ? params[j] : loaded;
        assertEquals(loaded, params[j]);
        assertEquals(expected, buffer[i]);
      }
    }
  }
  VerifyBoundsCheck(func, size);
}

(function TestAtomicCompareExchange() {
  print("TestAtomicCompareExchange");
  let wasmCmpExchange =
      GetAtomicCmpExchangeFunction(kExprI32AtomicCompareExchange);
  let i32 = new Uint32Array(memory.buffer);
  let params = [0x00000001, 0x00000555, 0x00099999, 0xffffffff];
  TestCmpExchange(wasmCmpExchange, i32, params, kMemtypeSize32);
})();

(function TestAtomicCompareExchange16U() {
  print("TestAtomicCompareExchange16U");
  let wasmCmpExchange =
      GetAtomicCmpExchangeFunction(kExprI32AtomicCompareExchange16U);
  let i16 = new Uint16Array(memory.buffer);
  let params = [0x0001, 0x0555, 0x9999];
  TestCmpExchange(wasmCmpExchange, i16, params, kMemtypeSize16);
})();

(function TestAtomicCompareExchange8U() {
  print("TestAtomicCompareExchange8U");
  let wasmCmpExchange =
      GetAtomicCmpExchangeFunction(kExprI32AtomicCompareExchange8U);
  let i8 = new Uint8Array(memory.buffer);
  let params = [0x01, 0x0d, 0xf9];
  TestCmpExchange(wasmCmpExchange, i8, params, kMemtypeSize8);
})();
