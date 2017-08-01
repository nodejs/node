// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

function unexpectedSuccess() {
  % AbortJS('unexpected success');
}

function unexpectedFail(error) {
  % AbortJS('unexpected fail: ' + error);
}

function assertEq(val, expected) {
  assertEquals(expected, val);
}
function assertArrayBuffer(val, expected) {
  assertTrue(val instanceof ArrayBuffer);
  assertEq(expected.length, val.byteLength);
  var input = new Int8Array(val);
  for (var i = 0; i < expected.length; i++) {
    assertEq(expected[i], input[i]);
  }
}
function wasmIsSupported() {
  return (typeof WebAssembly.Module) == 'function';
}
function assertErrorMessage(func, type, msg) {
  // TODO   assertThrows(func, type, msg);
  assertThrows(func, type);
}

let emptyModuleBinary = (() => {
  var builder = new WasmModuleBuilder();
  return new Int8Array(builder.toBuffer());
})();

let exportingModuleBinary = (() => {
  var builder = new WasmModuleBuilder();
  builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 42]).exportAs('f');
  return new Int8Array(builder.toBuffer());
})();

let importingModuleBinary = (() => {
  var builder = new WasmModuleBuilder();
  builder.addImport('', 'f', kSig_i_v);
  return new Int8Array(builder.toBuffer());
})();

let memoryImportingModuleBinary = (() => {
  var builder = new WasmModuleBuilder();
  builder.addImportedMemory('', 'my_memory');
  return new Int8Array(builder.toBuffer());
})();

let moduleBinaryImporting2Memories = (() => {
  var builder = new WasmModuleBuilder();
  builder.addImportedMemory('', 'memory1');
  builder.addImportedMemory('', 'memory2');
  return new Int8Array(builder.toBuffer());
})();

let moduleBinaryWithMemSectionAndMemImport = (() => {
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.addImportedMemory('', 'memory1');
  return new Int8Array(builder.toBuffer());
})();

// 'WebAssembly' data property on global object
let wasmDesc = Object.getOwnPropertyDescriptor(this, 'WebAssembly');
assertEq(typeof wasmDesc.value, 'object');
assertTrue(wasmDesc.writable);
assertFalse(wasmDesc.enumerable);
assertTrue(wasmDesc.configurable);

// 'WebAssembly' object
assertEq(WebAssembly, wasmDesc.value);
assertEq(String(WebAssembly), '[object WebAssembly]');

// 'WebAssembly.CompileError'
let compileErrorDesc =
    Object.getOwnPropertyDescriptor(WebAssembly, 'CompileError');
assertEq(typeof compileErrorDesc.value, 'function');
assertTrue(compileErrorDesc.writable);
assertFalse(compileErrorDesc.enumerable);
assertTrue(compileErrorDesc.configurable);
let CompileError = WebAssembly.CompileError;
assertEq(CompileError, compileErrorDesc.value);
assertEq(CompileError.length, 1);
assertEq(CompileError.name, 'CompileError');
let compileError = new CompileError;
assertTrue(compileError instanceof CompileError);
assertTrue(compileError instanceof Error);
assertFalse(compileError instanceof TypeError);
assertEq(compileError.message, '');
assertEq(new CompileError('hi').message, 'hi');

// 'WebAssembly.RuntimeError'
let runtimeErrorDesc =
    Object.getOwnPropertyDescriptor(WebAssembly, 'RuntimeError');
assertEq(typeof runtimeErrorDesc.value, 'function');
assertTrue(runtimeErrorDesc.writable);
assertFalse(runtimeErrorDesc.enumerable);
assertTrue(runtimeErrorDesc.configurable);
let RuntimeError = WebAssembly.RuntimeError;
assertEq(RuntimeError, runtimeErrorDesc.value);
assertEq(RuntimeError.length, 1);
assertEq(RuntimeError.name, 'RuntimeError');
let runtimeError = new RuntimeError;
assertTrue(runtimeError instanceof RuntimeError);
assertTrue(runtimeError instanceof Error);
assertFalse(runtimeError instanceof TypeError);
assertEq(runtimeError.message, '');
assertEq(new RuntimeError('hi').message, 'hi');

// 'WebAssembly.LinkError'
let linkErrorDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'LinkError');
assertEq(typeof linkErrorDesc.value, 'function');
assertTrue(linkErrorDesc.writable);
assertFalse(linkErrorDesc.enumerable);
assertTrue(linkErrorDesc.configurable);
let LinkError = WebAssembly.LinkError;
assertEq(LinkError, linkErrorDesc.value);
assertEq(LinkError.length, 1);
assertEq(LinkError.name, 'LinkError');
let linkError = new LinkError;
assertTrue(linkError instanceof LinkError);
assertTrue(linkError instanceof Error);
assertFalse(linkError instanceof TypeError);
assertEq(linkError.message, '');
assertEq(new LinkError('hi').message, 'hi');

// 'WebAssembly.Module' data property
let moduleDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Module');
assertEq(typeof moduleDesc.value, 'function');
assertTrue(moduleDesc.writable);
assertFalse(moduleDesc.enumerable);
assertTrue(moduleDesc.configurable);

// 'WebAssembly.Module' constructor function
let Module = WebAssembly.Module;
assertEq(Module, moduleDesc.value);
assertEq(Module.length, 1);
assertEq(Module.name, 'Module');
assertErrorMessage(
    () => Module(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(
    () => new Module(), TypeError, /requires more than 0 arguments/);
assertErrorMessage(
    () => new Module(undefined), TypeError,
    'first argument must be an ArrayBuffer or typed array object');
assertErrorMessage(
    () => new Module(1), TypeError,
    'first argument must be an ArrayBuffer or typed array object');
assertErrorMessage(
    () => new Module({}), TypeError,
    'first argument must be an ArrayBuffer or typed array object');
assertErrorMessage(
    () => new Module(new Uint8Array()), CompileError,
    /failed to match magic number/);
assertErrorMessage(
    () => new Module(new ArrayBuffer()), CompileError,
    /failed to match magic number/);
assertTrue(new Module(emptyModuleBinary) instanceof Module);
assertTrue(new Module(emptyModuleBinary.buffer) instanceof Module);

// 'WebAssembly.Module.prototype' data property
let moduleProtoDesc = Object.getOwnPropertyDescriptor(Module, 'prototype');
assertEq(typeof moduleProtoDesc.value, 'object');
assertFalse(moduleProtoDesc.writable);
assertFalse(moduleProtoDesc.enumerable);
assertFalse(moduleProtoDesc.configurable);

// 'WebAssembly.Module.prototype' object
let moduleProto = Module.prototype;
assertEq(moduleProto, moduleProtoDesc.value);
assertEq(String(moduleProto), '[object WebAssembly.Module]');
assertEq(Object.getPrototypeOf(moduleProto), Object.prototype);

// 'WebAssembly.Module' instance objects
let emptyModule = new Module(emptyModuleBinary);
let importingModule = new Module(importingModuleBinary);
let exportingModule = new Module(exportingModuleBinary);
assertEq(typeof emptyModule, 'object');
assertEq(String(emptyModule), '[object WebAssembly.Module]');
assertEq(Object.getPrototypeOf(emptyModule), moduleProto);

// 'WebAssembly.Module.imports' data property
let moduleImportsDesc = Object.getOwnPropertyDescriptor(Module, 'imports');
assertEq(typeof moduleImportsDesc.value, 'function');
assertTrue(moduleImportsDesc.writable);
assertFalse(moduleImportsDesc.enumerable);
assertTrue(moduleImportsDesc.configurable);

// 'WebAssembly.Module.imports' method
let moduleImports = moduleImportsDesc.value;
assertEq(moduleImports.length, 1);
assertErrorMessage(
    () => moduleImports(), TypeError, /requires more than 0 arguments/);
assertErrorMessage(
    () => moduleImports(undefined), TypeError,
    /first argument must be a WebAssembly.Module/);
assertErrorMessage(
    () => moduleImports({}), TypeError,
    /first argument must be a WebAssembly.Module/);
var arr = moduleImports(new Module(emptyModuleBinary));
assertTrue(arr instanceof Array);
assertEq(arr.length, 0);
let importingModuleBinary2 = (() => {
  var text =
      '(module (func (import "a" "b")) (memory (import "c" "d") 1) (table (import "e" "f") 1 anyfunc) (global (import "g" "⚡") i32))'
  let builder = new WasmModuleBuilder();
  builder.addImport('a', 'b', kSig_i_i);
  builder.addImportedMemory('c', 'd');
  builder.addImportedTable('e', 'f');
  builder.addImportedGlobal('g', 'x', kWasmI32);
  return new Int8Array(builder.toBuffer());
})();
var arr = moduleImports(new Module(importingModuleBinary2));
assertTrue(arr instanceof Array);
assertEq(arr.length, 4);
assertEq(arr[0].kind, 'function');
assertEq(arr[0].module, 'a');
assertEq(arr[0].name, 'b');
assertEq(arr[1].kind, 'memory');
assertEq(arr[1].module, 'c');
assertEq(arr[1].name, 'd');
assertEq(arr[2].kind, 'table');
assertEq(arr[2].module, 'e');
assertEq(arr[2].name, 'f');
assertEq(arr[3].kind, 'global');
assertEq(arr[3].module, 'g');
assertEq(arr[3].name, 'x');

// 'WebAssembly.Module.exports' data property
let moduleExportsDesc = Object.getOwnPropertyDescriptor(Module, 'exports');
assertEq(typeof moduleExportsDesc.value, 'function');
assertTrue(moduleExportsDesc.writable);
assertFalse(moduleExportsDesc.enumerable);
assertTrue(moduleExportsDesc.configurable);

// 'WebAssembly.Module.exports' method
let moduleExports = moduleExportsDesc.value;
assertEq(moduleExports.length, 1);
assertErrorMessage(
    () => moduleExports(), TypeError, /requires more than 0 arguments/);
assertErrorMessage(
    () => moduleExports(undefined), TypeError,
    /first argument must be a WebAssembly.Module/);
assertErrorMessage(
    () => moduleExports({}), TypeError,
    /first argument must be a WebAssembly.Module/);
var arr = moduleExports(emptyModule);
assertTrue(arr instanceof Array);
assertEq(arr.length, 0);
let exportingModuleBinary2 = (() => {
  var text =
      '(module (func (export "a")) (memory (export "b") 1) (table (export "c") 1 anyfunc) (global (export "⚡") i32 (i32.const 0)))';
  let builder = new WasmModuleBuilder();
  builder.addFunction('foo', kSig_v_v).addBody([]).exportAs('a');
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('b');
  builder.setFunctionTableLength(1);
  builder.addExportOfKind('c', kExternalTable, 0);
  var o = builder.addGlobal(kWasmI32, false).exportAs('x');
  return new Int8Array(builder.toBuffer());
})();
var arr = moduleExports(new Module(exportingModuleBinary2));
assertTrue(arr instanceof Array);
assertEq(arr.length, 4);
assertEq(arr[0].kind, 'function');
assertEq(arr[0].name, 'a');
assertEq(arr[1].kind, 'memory');
assertEq(arr[1].name, 'b');
assertEq(arr[2].kind, 'table');
assertEq(arr[2].name, 'c');
assertEq(arr[3].kind, 'global');
assertEq(arr[3].name, 'x');

// 'WebAssembly.Module.customSections' data property
let moduleCustomSectionsDesc =
    Object.getOwnPropertyDescriptor(Module, 'customSections');
assertEq(typeof moduleCustomSectionsDesc.value, 'function');
assertEq(moduleCustomSectionsDesc.writable, true);
assertEq(moduleCustomSectionsDesc.enumerable, false);
assertEq(moduleCustomSectionsDesc.configurable, true);

let moduleCustomSections = moduleCustomSectionsDesc.value;
assertEq(moduleCustomSections.length, 2);
assertErrorMessage(
    () => moduleCustomSections(), TypeError, /requires more than 0 arguments/);
assertErrorMessage(
    () => moduleCustomSections(undefined), TypeError,
    /first argument must be a WebAssembly.Module/);
assertErrorMessage(
    () => moduleCustomSections({}), TypeError,
    /first argument must be a WebAssembly.Module/);
var arr = moduleCustomSections(emptyModule, 'x');
assertEq(arr instanceof Array, true);
assertEq(arr.length, 0);

assertErrorMessage(
    () => moduleCustomSections(1), TypeError,
    'first argument must be a WebAssembly.Module');
assertErrorMessage(
    () => moduleCustomSections(emptyModule), TypeError,
    'second argument must be a String');
assertErrorMessage(
    () => moduleCustomSections(emptyModule, 3), TypeError,
    'second argument must be a String');

let customSectionModuleBinary2 = (() => {
  let builder = new WasmModuleBuilder();
  builder.addCustomSection('x', [2]);
  builder.addCustomSection('foo', [66, 77]);
  builder.addCustomSection('foo', [91, 92, 93]);
  builder.addCustomSection('fox', [99, 99, 99]);
  return new Int8Array(builder.toBuffer());
})();
var arr = moduleCustomSections(new Module(customSectionModuleBinary2), 'x');
assertEq(arr instanceof Array, true);
assertEq(arr.length, 1);
assertArrayBuffer(arr[0], [2]);
var arr = moduleCustomSections(new Module(customSectionModuleBinary2), 'foo');
assertEq(arr instanceof Array, true);
assertEq(arr.length, 2);
assertArrayBuffer(arr[0], [66, 77]);
assertArrayBuffer(arr[1], [91, 92, 93]);
var arr = moduleCustomSections(new Module(customSectionModuleBinary2), 'bar');
assertEq(arr instanceof Array, true);
assertEq(arr.length, 0);

// 'WebAssembly.Instance' data property
let instanceDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Instance');
assertEq(typeof instanceDesc.value, 'function');
assertTrue(instanceDesc.writable);
assertFalse(instanceDesc.enumerable);
assertTrue(instanceDesc.configurable);

// 'WebAssembly.Instance' constructor function
let Instance = WebAssembly.Instance;
assertEq(Instance, instanceDesc.value);
assertEq(Instance.length, 1);
assertEq(Instance.name, 'Instance');

assertErrorMessage(
    () => Instance(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(
    () => new Instance(1), TypeError,
    'first argument must be a WebAssembly.Module');
assertErrorMessage(
    () => new Instance({}), TypeError,
    'first argument must be a WebAssembly.Module');
assertErrorMessage(
    () => new Instance(emptyModule, null), TypeError,
    'second argument must be an object');
assertErrorMessage(() => new Instance(importingModule, null), TypeError, '');
assertErrorMessage(
    () => new Instance(importingModule, undefined), TypeError, '');
assertErrorMessage(
    () => new Instance(importingModule, {'': {g: () => {}}}), LinkError, '');
assertErrorMessage(
    () => new Instance(importingModule, {t: {f: () => {}}}), TypeError, '');

assertTrue(new Instance(emptyModule) instanceof Instance);
assertTrue(new Instance(emptyModule, {}) instanceof Instance);

// 'WebAssembly.Instance.prototype' data property
let instanceProtoDesc = Object.getOwnPropertyDescriptor(Instance, 'prototype');
assertEq(typeof instanceProtoDesc.value, 'object');
assertFalse(instanceProtoDesc.writable);
assertFalse(instanceProtoDesc.enumerable);
assertFalse(instanceProtoDesc.configurable);

// 'WebAssembly.Instance.prototype' object
let instanceProto = Instance.prototype;
assertEq(instanceProto, instanceProtoDesc.value);
assertEq(String(instanceProto), '[object WebAssembly.Instance]');
assertEq(Object.getPrototypeOf(instanceProto), Object.prototype);

// 'WebAssembly.Instance' instance objects
let exportingInstance = new Instance(exportingModule);
assertEq(typeof exportingInstance, 'object');
assertEq(String(exportingInstance), '[object WebAssembly.Instance]');
assertEq(Object.getPrototypeOf(exportingInstance), instanceProto);

// 'WebAssembly.Instance' 'exports' data property
let instanceExportsDesc =
    Object.getOwnPropertyDescriptor(exportingInstance, 'exports');
assertEq(typeof instanceExportsDesc.value, 'object');
assertTrue(instanceExportsDesc.writable);
assertTrue(instanceExportsDesc.enumerable);
assertTrue(instanceExportsDesc.configurable);

exportsObj = exportingInstance.exports;
assertEq(typeof exportsObj, 'object');
assertFalse(Object.isExtensible(exportsObj));
assertEq(Object.getPrototypeOf(exportsObj), null);
assertEq(Object.keys(exportsObj).join(), 'f');

// Exported WebAssembly functions
let f = exportingInstance.exports.f;
assertTrue(f instanceof Function);
assertEq(f.length, 0);
assertTrue('name' in f);
assertEq(Function.prototype.call.call(f), 42);
assertErrorMessage(() => new f(), TypeError, /is not a constructor/);

// 'WebAssembly.Memory' data property
let memoryDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Memory');
assertEq(typeof memoryDesc.value, 'function');
assertTrue(memoryDesc.writable);
assertFalse(memoryDesc.enumerable);
assertTrue(memoryDesc.configurable);

// 'WebAssembly.Memory' constructor function
let Memory = WebAssembly.Memory;
assertEq(Memory, memoryDesc.value);
assertEq(Memory.length, 1);
assertEq(Memory.name, 'Memory');
assertErrorMessage(
    () => Memory(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(
    () => new Memory(1), TypeError,
    'first argument must be a memory descriptor');
assertErrorMessage(
    () => new Memory({initial: {valueOf() { throw new Error('here') }}}), Error,
    'here');
assertErrorMessage(
    () => new Memory({initial: -1}), RangeError, /bad Memory initial size/);
assertErrorMessage(
    () => new Memory({initial: Math.pow(2, 32)}), RangeError,
    /bad Memory initial size/);
assertErrorMessage(
    () => new Memory({initial: 1, maximum: Math.pow(2, 32) / Math.pow(2, 14)}),
    RangeError, /bad Memory maximum size/);
assertErrorMessage(
    () => new Memory({initial: 2, maximum: 1}), RangeError,
    /bad Memory maximum size/);
assertErrorMessage(
    () => new Memory({maximum: -1}), RangeError, /bad Memory maximum size/);
assertTrue(new Memory({initial: 1}) instanceof Memory);
assertEq(new Memory({initial: 1.5}).buffer.byteLength, kPageSize);

// 'WebAssembly.Memory.prototype' data property
let memoryProtoDesc = Object.getOwnPropertyDescriptor(Memory, 'prototype');
assertEq(typeof memoryProtoDesc.value, 'object');
assertFalse(memoryProtoDesc.writable);
assertFalse(memoryProtoDesc.enumerable);
assertFalse(memoryProtoDesc.configurable);

// 'WebAssembly.Memory.prototype' object
let memoryProto = Memory.prototype;
assertEq(memoryProto, memoryProtoDesc.value);
assertEq(String(memoryProto), '[object WebAssembly.Memory]');
assertEq(Object.getPrototypeOf(memoryProto), Object.prototype);

// 'WebAssembly.Memory' instance objects
let mem1 = new Memory({initial: 1});
assertEq(typeof mem1, 'object');
assertEq(String(mem1), '[object WebAssembly.Memory]');
assertEq(Object.getPrototypeOf(mem1), memoryProto);

// 'WebAssembly.Memory.prototype.buffer' accessor property
let bufferDesc = Object.getOwnPropertyDescriptor(memoryProto, 'buffer');
assertEq(typeof bufferDesc.get, 'function');
assertEq(bufferDesc.set, undefined);
assertFalse(bufferDesc.enumerable);
assertTrue(bufferDesc.configurable);

// 'WebAssembly.Memory.prototype.buffer' getter
let bufferGetter = bufferDesc.get;
assertErrorMessage(
    () => bufferGetter.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(
    () => bufferGetter.call({}), TypeError, /called on incompatible Object/);
assertTrue(bufferGetter.call(mem1) instanceof ArrayBuffer);
assertEq(bufferGetter.call(mem1).byteLength, kPageSize);

// 'WebAssembly.Memory.prototype.grow' data property
let memGrowDesc = Object.getOwnPropertyDescriptor(memoryProto, 'grow');
assertEq(typeof memGrowDesc.value, 'function');
assertFalse(memGrowDesc.enumerable);
assertTrue(memGrowDesc.configurable);

// 'WebAssembly.Memory.prototype.grow' method

let memGrow = memGrowDesc.value;
assertEq(memGrow.length, 1);
assertErrorMessage(
    () => memGrow.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(
    () => memGrow.call({}), TypeError, /called on incompatible Object/);
assertErrorMessage(
    () => memGrow.call(mem1, -1), RangeError, /bad Memory grow delta/);
assertErrorMessage(
    () => memGrow.call(mem1, Math.pow(2, 32)), RangeError,
    /bad Memory grow delta/);
var mem = new Memory({initial: 1, maximum: 2});
var buf = mem.buffer;
assertEq(buf.byteLength, kPageSize);
assertEq(mem.grow(0), 1);
assertTrue(buf !== mem.buffer);
assertEq(buf.byteLength, 0);
buf = mem.buffer;
assertEq(buf.byteLength, kPageSize);
assertEq(mem.grow(1), 1);
assertTrue(buf !== mem.buffer);
assertEq(buf.byteLength, 0);
buf = mem.buffer;
assertEq(buf.byteLength, 2 * kPageSize);
assertErrorMessage(() => mem.grow(1), Error, /failed to grow memory/);
assertEq(buf, mem.buffer);

// 'WebAssembly.Table' data property
let tableDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Table');
assertEq(typeof tableDesc.value, 'function');
assertTrue(tableDesc.writable);
assertFalse(tableDesc.enumerable);
assertTrue(tableDesc.configurable);

// 'WebAssembly.Table' constructor function
let Table = WebAssembly.Table;
assertEq(Table, tableDesc.value);
assertEq(Table.length, 1);
assertEq(Table.name, 'Table');
assertErrorMessage(
    () => Table(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(
    () => new Table(1), TypeError, 'first argument must be a table descriptor');
assertErrorMessage(
    () => new Table({initial: 1, element: 1}), TypeError, /must be "anyfunc"/);
assertErrorMessage(
    () => new Table({initial: 1, element: 'any'}), TypeError,
    /must be "anyfunc"/);
assertErrorMessage(
    () => new Table({initial: 1, element: {valueOf() { return 'anyfunc' }}}),
    TypeError, /must be "anyfunc"/);
assertErrorMessage(
    () => new Table(
        {initial: {valueOf() { throw new Error('here') }}, element: 'anyfunc'}),
    Error, 'here');
assertErrorMessage(
    () => new Table({initial: -1, element: 'anyfunc'}), RangeError,
    /bad Table initial size/);
assertErrorMessage(
    () => new Table({initial: Math.pow(2, 32), element: 'anyfunc'}), RangeError,
    /bad Table initial size/);
assertErrorMessage(
    () => new Table({initial: 2, maximum: 1, element: 'anyfunc'}), RangeError,
    /bad Table maximum size/);
assertErrorMessage(
    () => new Table({initial: 2, maximum: Math.pow(2, 32), element: 'anyfunc'}),
    RangeError, /bad Table maximum size/);
assertTrue(new Table({initial: 1, element: 'anyfunc'}) instanceof Table);
assertTrue(new Table({initial: 1.5, element: 'anyfunc'}) instanceof Table);
assertTrue(
    new Table({initial: 1, maximum: 1.5, element: 'anyfunc'}) instanceof Table);
// TODO:maximum assertTrue(new Table({initial:1, maximum:Math.pow(2,32)-1,
// element:"anyfunc"}) instanceof Table);

// 'WebAssembly.Table.prototype' data property
let tableProtoDesc = Object.getOwnPropertyDescriptor(Table, 'prototype');
assertEq(typeof tableProtoDesc.value, 'object');
assertFalse(tableProtoDesc.writable);
assertFalse(tableProtoDesc.enumerable);
assertFalse(tableProtoDesc.configurable);

// 'WebAssembly.Table.prototype' object
let tableProto = Table.prototype;
assertEq(tableProto, tableProtoDesc.value);
assertEq(String(tableProto), '[object WebAssembly.Table]');
assertEq(Object.getPrototypeOf(tableProto), Object.prototype);

// 'WebAssembly.Table' instance objects
let tbl1 = new Table({initial: 2, element: 'anyfunc'});
assertEq(typeof tbl1, 'object');
assertEq(String(tbl1), '[object WebAssembly.Table]');
assertEq(Object.getPrototypeOf(tbl1), tableProto);

// 'WebAssembly.Table.prototype.length' accessor data property
let lengthDesc = Object.getOwnPropertyDescriptor(tableProto, 'length');
assertEq(typeof lengthDesc.get, 'function');
assertEq(lengthDesc.set, undefined);
assertFalse(lengthDesc.enumerable);
assertTrue(lengthDesc.configurable);

// 'WebAssembly.Table.prototype.length' getter
let lengthGetter = lengthDesc.get;
assertEq(lengthGetter.length, 0);
assertErrorMessage(
    () => lengthGetter.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(
    () => lengthGetter.call({}), TypeError, /called on incompatible Object/);
assertEq(typeof lengthGetter.call(tbl1), 'number');
assertEq(lengthGetter.call(tbl1), 2);

// 'WebAssembly.Table.prototype.get' data property
let getDesc = Object.getOwnPropertyDescriptor(tableProto, 'get');
assertEq(typeof getDesc.value, 'function');
assertFalse(getDesc.enumerable);
assertTrue(getDesc.configurable);

// 'WebAssembly.Table.prototype.get' method
let get = getDesc.value;
assertEq(get.length, 1);
assertErrorMessage(
    () => get.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(
    () => get.call({}), TypeError, /called on incompatible Object/);
assertEq(get.call(tbl1, 0), null);
assertEq(get.call(tbl1, 1), null);
assertEq(get.call(tbl1, 1.5), null);
assertErrorMessage(() => get.call(tbl1, 2), RangeError, /bad Table get index/);
assertErrorMessage(
    () => get.call(tbl1, 2.5), RangeError, /bad Table get index/);
assertErrorMessage(() => get.call(tbl1, -1), RangeError, /bad Table get index/);
// TODO assertErrorMessage(() => get.call(tbl1, Math.pow(2,33)), RangeError,
// /bad Table get index/);
assertErrorMessage(
    () => get.call(tbl1, {valueOf() { throw new Error('hi') }}), Error, 'hi');

// 'WebAssembly.Table.prototype.set' data property
let setDesc = Object.getOwnPropertyDescriptor(tableProto, 'set');
assertEq(typeof setDesc.value, 'function');
assertFalse(setDesc.enumerable);
assertTrue(setDesc.configurable);

// 'WebAssembly.Table.prototype.set' method
let set = setDesc.value;
assertEq(set.length, 2);
assertErrorMessage(
    () => set.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(
    () => set.call({}), TypeError, /called on incompatible Object/);
assertErrorMessage(
    () => set.call(tbl1, 0), TypeError, /requires more than 1 argument/);
assertErrorMessage(
    () => set.call(tbl1, 2, null), RangeError, /bad Table set index/);
assertErrorMessage(
    () => set.call(tbl1, -1, null), RangeError, /bad Table set index/);
// TODO assertErrorMessage(() => set.call(tbl1, Math.pow(2,33), null),
// RangeError, /bad Table set index/);
assertErrorMessage(
    () => set.call(tbl1, 0, undefined), TypeError,
    /can only assign WebAssembly exported functions to Table/);
assertErrorMessage(
    () => set.call(tbl1, 0, {}), TypeError,
    /can only assign WebAssembly exported functions to Table/);
assertErrorMessage(() => set.call(tbl1, 0, function() {
}), TypeError, /can only assign WebAssembly exported functions to Table/);
assertErrorMessage(
    () => set.call(tbl1, 0, Math.sin), TypeError,
    /can only assign WebAssembly exported functions to Table/);
assertErrorMessage(
    () => set.call(tbl1, {valueOf() { throw Error('hai') }}, null), Error,
    'hai');
assertEq(set.call(tbl1, 0, null), undefined);
assertEq(set.call(tbl1, 1, null), undefined);

// 'WebAssembly.Table.prototype.grow' data property
let tblGrowDesc = Object.getOwnPropertyDescriptor(tableProto, 'grow');
assertEq(typeof tblGrowDesc.value, 'function');
assertFalse(tblGrowDesc.enumerable);
assertTrue(tblGrowDesc.configurable);

// 'WebAssembly.Table.prototype.grow' method
let tblGrow = tblGrowDesc.value;
assertEq(tblGrow.length, 1);
assertErrorMessage(
    () => tblGrow.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(
    () => tblGrow.call({}), TypeError, /called on incompatible Object/);
assertErrorMessage(
    () => tblGrow.call(tbl1, -1), RangeError, /bad Table grow delta/);
assertErrorMessage(
    () => tblGrow.call(tbl1, Math.pow(2, 32)), RangeError,
    /bad Table grow delta/);
var tbl = new Table({element: 'anyfunc', initial: 1, maximum: 2});
assertEq(tbl.length, 1);
assertEq(tbl.grow(0), 1);
assertEq(tbl.length, 1);
assertEq(tbl.grow(1), 1);
assertEq(tbl.length, 2);
assertErrorMessage(() => tbl.grow(1), Error, /failed to grow table/);

// 'WebAssembly.validate' function
assertErrorMessage(() => WebAssembly.validate(), TypeError);
assertErrorMessage(() => WebAssembly.validate('hi'), TypeError);
assertTrue(WebAssembly.validate(emptyModuleBinary));
// TODO: other ways for validate to return false.
assertFalse(WebAssembly.validate(moduleBinaryImporting2Memories));
assertFalse(WebAssembly.validate(moduleBinaryWithMemSectionAndMemImport));

// 'WebAssembly.compile' data property
let compileDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'compile');
assertEq(typeof compileDesc.value, 'function');
assertTrue(compileDesc.writable);
assertFalse(compileDesc.enumerable);
assertTrue(compileDesc.configurable);

// 'WebAssembly.compile' function
let compile = WebAssembly.compile;
assertEq(compile, compileDesc.value);
assertEq(compile.length, 1);
assertEq(compile.name, 'compile');
function assertCompileError(args, err, msg) {
  var error = null;
  assertPromiseResult(compile(...args), unexpectedSuccess, error => {
    assertTrue(error instanceof err);
    // TODO  assertTrue(Boolean(error.message.match(msg)));
  });
}
assertCompileError([], TypeError, /requires more than 0 arguments/);
assertCompileError(
    [undefined], TypeError,
    /first argument must be an ArrayBuffer or typed array object/);
assertCompileError(
    [1], TypeError,
    /first argument must be an ArrayBuffer or typed array object/);
assertCompileError(
    [{}], TypeError,
    /first argument must be an ArrayBuffer or typed array object/);
assertCompileError(
    [new Uint8Array()], CompileError, /BufferSource argument is empty/);
assertCompileError(
    [new ArrayBuffer()], CompileError, /BufferSource argument is empty/);
assertCompileError(
    [new Uint8Array('hi!')], CompileError, /failed to match magic number/);
assertCompileError(
    [new ArrayBuffer('hi!')], CompileError, /failed to match magic number/);

function assertCompileSuccess(bytes) {
  var module = null;
  assertPromiseResult(compile(bytes), m => assertTrue(m instanceof Module));
}
assertCompileSuccess(emptyModuleBinary);
assertCompileSuccess(emptyModuleBinary.buffer);

// 'WebAssembly.instantiate' data property
let instantiateDesc =
    Object.getOwnPropertyDescriptor(WebAssembly, 'instantiate');
assertEq(typeof instantiateDesc.value, 'function');
assertTrue(instantiateDesc.writable);
assertFalse(instantiateDesc.enumerable);
assertTrue(instantiateDesc.configurable);

// 'WebAssembly.instantiate' function
let instantiate = WebAssembly.instantiate;
assertEq(instantiate, instantiateDesc.value);
assertEq(instantiate.length, 1);
assertEq(instantiate.name, 'instantiate');
function assertInstantiateError(args, err, msg) {
  var error = null;
  assertPromiseResult(instantiate(...args), unexpectedSuccess, error => {
    assertTrue(error instanceof err);
    // TODO assertTrue(Boolean(error.message.match(msg)));
  });
}
var scratch_memory = new WebAssembly.Memory(new ArrayBuffer(10));
assertInstantiateError([], TypeError, /requires more than 0 arguments/);
assertInstantiateError(
    [undefined], TypeError, /first argument must be a BufferSource/);
assertInstantiateError([1], TypeError, /first argument must be a BufferSource/);
assertInstantiateError(
    [{}], TypeError, /first argument must be a BufferSource/);
assertInstantiateError(
    [new Uint8Array()], CompileError, /failed to match magic number/);
assertInstantiateError(
    [new ArrayBuffer()], CompileError, /failed to match magic number/);
assertInstantiateError(
    [new Uint8Array('hi!')], CompileError, /failed to match magic number/);
assertInstantiateError(
    [new ArrayBuffer('hi!')], CompileError, /failed to match magic number/);
assertInstantiateError(
    [importingModule], TypeError, /second argument must be an object/);
assertInstantiateError(
    [importingModule, null], TypeError, /second argument must be an object/);
assertInstantiateError(
    [importingModuleBinary, null], TypeError,
    /second argument must be an object/);
assertInstantiateError(
    [emptyModule, null], TypeError, /first argument must be a BufferSource/);
assertInstantiateError(
    [importingModuleBinary, null], TypeError, /TODO: error messages?/);
assertInstantiateError(
    [importingModuleBinary, undefined], TypeError, /TODO: error messages?/);
assertInstantiateError(
    [importingModuleBinary, {}], TypeError, /TODO: error messages?/);
assertInstantiateError(
    [importingModuleBinary, {'': {g: () => {}}}], LinkError,
    /TODO: error messages?/);
assertInstantiateError(
    [importingModuleBinary, {t: {f: () => {}}}], TypeError,
    /TODO: error messages?/);
assertInstantiateError(
    [memoryImportingModuleBinary, null], TypeError, /TODO: error messages?/);
assertInstantiateError(
    [memoryImportingModuleBinary, undefined], TypeError,
    /TODO: error messages?/);
assertInstantiateError(
    [memoryImportingModuleBinary, {}], TypeError, /TODO: error messages?/);
assertInstantiateError(
    [memoryImportingModuleBinary, {'mod': {'my_memory': scratch_memory}}],
    TypeError, /TODO: error messages?/);
assertInstantiateError(
    [memoryImportingModuleBinary, {'': {'memory': scratch_memory}}], LinkError,
    /TODO: error messages?/);

function assertInstantiateSuccess(module_or_bytes, imports) {
  var result = null;
  assertPromiseResult(instantiate(module_or_bytes, imports), result => {
    if (module_or_bytes instanceof Module) {
      assertTrue(result instanceof Instance);
    } else {
      assertTrue(result.module instanceof Module);
      assertTrue(result.instance instanceof Instance);
    }
  });
}
assertInstantiateSuccess(emptyModule);
assertInstantiateSuccess(emptyModuleBinary);
assertInstantiateSuccess(emptyModuleBinary.buffer);
assertInstantiateSuccess(importingModule, {'': {f: () => {}}});
assertInstantiateSuccess(importingModuleBinary, {'': {f: () => {}}});
assertInstantiateSuccess(importingModuleBinary.buffer, {'': {f: () => {}}});
assertInstantiateSuccess(
    memoryImportingModuleBinary, {'': {'my_memory': scratch_memory}});
