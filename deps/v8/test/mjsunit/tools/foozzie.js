// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --correctness-fuzzer-suppressions
// Files: tools/clusterfuzz/foozzie/v8_mock.js

// Test foozzie mocks for differential fuzzing.

// Deterministic Math.random.
assertEquals(0.7098480789645691, Math.random());
assertEquals(0.9742682568175951, Math.random());
assertEquals(0.20008059867222983, Math.random());

// Deterministic date.
assertEquals(1477662728698, Date.now());
assertEquals(1477662728701, Date.now());
assertEquals(1477662728705, new Date().getTime());
assertEquals(710, new Date.prototype.constructor().getUTCMilliseconds());

// Deterministic arguments in constructor keep working.
assertEquals(819134640000,
             new Date('December 17, 1995 03:24:00 GMT+1000').getTime());

// Deterministic DateTimeFormat.
if (this.Intl) {
  const df = new Intl.DateTimeFormat(undefined, {fractionalSecondDigits: 3});
  assertEquals('004', df.format());
  assertEquals('004', df.formatToParts()[0].value);
}

// Dummy performance methods.
assertEquals(undefined, performance.now());
assertEquals(undefined, performance.mark("a mark"));
assertEquals(undefined, performance.measure("a measure"));
assertEquals(undefined, performance.measureMemory());
assertEquals(undefined, new this.constructor().performance.now());
assertEquals(undefined, new this.constructor().performance.mark());
assertEquals(undefined, new this.constructor().performance.measure());
assertEquals(undefined, new this.constructor().performance.measureMemory());

// Worker messages follow a predefined deterministic pattern.
const worker = new Worker(``, {type: 'string'});
assertEquals(0, worker.getMessage());
assertEquals(-1, worker.getMessage());

// NaN patterns in typed arrays are mocked out. Test that we get no
// difference between unoptimized and optimized code.
function testSameOptimized(intArrayType, pattern, create_fun) {
  const expected = new intArrayType(pattern);
  %PrepareFunctionForOptimization(create_fun);
  assertEquals(expected, create_fun());
  %OptimizeFunctionOnNextCall(create_fun);
  assertEquals(expected, create_fun());
}

function testArrayType(intArrayType, floatArrayType, pattern) {
  // Test passing NaNs to constructor with array.
  let create = function() {
    return new intArrayType(new floatArrayType([-NaN]).buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  // Test passing NaNs to constructor with iterator.
  create = function() {
    const iter = function*(){ yield* [-NaN]; }();
    return new intArrayType(new floatArrayType(iter).buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  // Test setting NaN property.
  create = function() {
    const arr = new floatArrayType(1);
    arr[0] = -NaN;
    return new intArrayType(arr.buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  // Test passing NaN using set.
  create = function() {
    const arr = new floatArrayType(1);
    arr.set([-NaN], 0);
    return new intArrayType(arr.buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  // Pass NaN via set using an iterable type.
  create = function() {
    const arr = new floatArrayType(1);
    arr.set({ length: 1 }, 0);
    return new intArrayType(arr.buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  create = function() {
    const arr = new floatArrayType(1);
    Object.defineProperty(arr, 0, { value: undefined });
    return new intArrayType(arr.buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  create = function() {
    const arr = new floatArrayType(1);
    Object.defineProperty(arr, 0, { value: () => { return undefined; } });
    return new intArrayType(arr.buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
  create = function() {
    const arr = new floatArrayType(1);
    arr.fill(undefined);
    return new intArrayType(arr.buffer);
  };
  testSameOptimized(intArrayType, pattern, create);
}

var isBigEndian = new Uint8Array(new Uint16Array([0xABCD]).buffer)[0] === 0xAB;
testArrayType(Uint16Array, Float16Array, [15360]);
testArrayType(Uint32Array, Float32Array, [1065353216]);
if (isBigEndian){
  testArrayType(Uint16Array, Float32Array, [16256, 0]);
  testArrayType(Uint16Array, Float64Array, [16368, 0, 0, 0]);
  testArrayType(Uint32Array, Float64Array, [1072693248, 0]);
}
else {
  testArrayType(Uint16Array, Float32Array, [0, 16256]);
  testArrayType(Uint16Array, Float64Array, [0, 0, 0, 16368]);
  testArrayType(Uint32Array, Float64Array, [0, 1072693248]);
}

// We stub out float array property setting. Test that we don't stub
// out altering other objects.
const someObject = {};
Object.defineProperty(someObject, 0, { value: undefined });
assertEquals(undefined, someObject[0]);

// Test that we preserve non-NaN values and don't alter NaN properties.
function testPreservation(arrayType) {
  const arr = new arrayType(4);
  Object.defineProperty(arr, 0, { value: 42 })
  assertEquals(42, arr[0]);
  Object.defineProperty(arr, "a", { get() { return 43; } })
  assertEquals(43, arr["a"]);
  arr.set([44], 1);
  assertEquals(44, arr[1]);
  arr.fill(45, 2, 4);
  assertEquals(45, arr[2]);
  assertEquals(45, arr[3]);
}
testPreservation(Float16Array);
testPreservation(Float32Array);
testPreservation(Float64Array);

// Test that DataView has the same NaN patterns with optimized and
// unoptimized code.
function createDataView(array) {
  return () => {
    const view = new DataView(array.buffer);
    view.setFloat64(0, Math.PI);
    view.setFloat64(8, -undefined);
    view.setFloat32(16, Math.fround(Math.PI));
    view.setFloat32(20, -undefined);
    view.setFloat16(24, Math.f16round(Math.PI));
    view.setFloat16(26, -undefined);
    assertEquals(Math.PI, view.getFloat64(0));
    assertEquals(Math.fround(Math.PI), view.getFloat32(16));
    assertEquals(Math.f16round(Math.PI), view.getFloat16(24));
    return array;
  };
}

if (isBigEndian){
  const expected_array = [
    16393, 8699, 21572, 11544, 16368, 0, 0, 0,
    16457, 4059, 16256, 0, 16968, 15360,
  ];
  testSameOptimized(
      Uint16Array, expected_array, createDataView(new Uint16Array(14)));
} else {
  const expected_array = [
    2368, 64289, 17492, 6189, 61503, 0, 0, 0,
    18752, 56079, 32831, 0, 18498, 60,
  ];
  testSameOptimized(
      Uint16Array, expected_array, createDataView(new Uint16Array(14)));
}

if (isBigEndian){
  const expected_array = [
    1074340347, 1413754136, 1072693248, 0, 1078530011, 1065353216, 1112030208,
  ];
  testSameOptimized(
      Uint32Array, expected_array, createDataView(new Uint32Array(7)));
} else {
  const expected_array = [
    4213246272, 405619796, 61503, 0, 3675212096, 32831, 3950658,
  ];
  testSameOptimized(
      Uint32Array, expected_array, createDataView(new Uint32Array(7)));
}

// Realm.eval is just eval.
assertEquals(1477662728716, Realm.eval(Realm.create(), `Date.now()`));

// Test mocked Atomics.waitAsync.
let then_called = false;
Atomics.waitAsync().value.then(() => {then_called = true;});
assertEquals(true, then_called);

// Test .caller access is neutralized.
function callee() {
  assertEquals(null, callee.caller);
}
function caller() {
  callee();
}
caller();

// Neutralized APIs.
let object = {'foo': 42}
assertEquals(d8.serializer.serialize(object), object)
assertEquals(d8.serializer.deserialize(object), object)
assertEquals(d8.profiler.setOnProfileEndListener(object), object)
assertEquals(d8.profiler.triggerSample(object), object)
assertEquals(d8.log.getAndStop(object), object)
