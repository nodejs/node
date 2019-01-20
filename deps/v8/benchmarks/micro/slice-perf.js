// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kIterations = 1000000;
const kIterationShort = 10000;
const kArraySize = 64;

let smi_array = [];
for (let i = 0; i < kArraySize; ++i) smi_array[i] = Math.floor(Math.random() * 100);

let start = performance.now();
for (let x = 0; x < kIterations; ++x) {
  smi_array.slice(0);
}
let stop = performance.now();
print("smi_array copy: " + (Math.floor((stop - start)*10)/10) + " ms");

start = performance.now();
for (let x = 0; x < kIterations; ++x) {
  smi_array.slice(x % kArraySize);
}
stop = performance.now();
print("smi_array: " + (Math.floor((stop - start)*10)/10) + " ms");

let double_array = [];
for (let i = 0; i < kArraySize; ++i) double_array[i] = Math.random() * 100;
start = performance.now();
for (let x = 0; x < kIterations; ++x) {
  double_array.slice(x % kArraySize);
}
stop = performance.now();
print("double_array: " + (Math.floor((stop - start)*10)/10) + " ms");

let object_array = [];
for (let i = 0; i < kArraySize; ++i) object_array[i] = new Object();
start = performance.now();
for (let x = 0; x < kIterations; ++x) {
  object_array.slice(x % kArraySize);
}
stop = performance.now();
print("object_array: " + (Math.floor((stop - start)*10)/10) + " ms");

let dictionary_array = [];
for (let i = 0; i < kArraySize; ++i) dictionary_array[i] = new Object();
dictionary_array[100000] = new Object();
start = performance.now();
for (let x = 0; x < kIterationShort; ++x) {
  dictionary_array.slice(x % kArraySize);
}
stop = performance.now();
print("dictionary: " + (Math.floor((stop - start)*10)/10) + " ms");

let arguments_array;
function sloppy() {
  arguments_array = arguments;
}
sloppy.apply(null, smi_array);
start = performance.now();
for (let x = 0; x < kIterations; ++x) {
  let r = Array.prototype.slice.call(arguments_array, x % kArraySize);
}
stop = performance.now();
print("arguments_array (sloppy): " + (Math.floor((stop - start)*10)/10) + " ms");

function sloppy2 (a) {
  arguments_array = arguments;
}
sloppy2.apply(null, smi_array);
start = performance.now();
for (let x = 0; x < kIterations; ++x) {
  Array.prototype.slice.call(arguments_array, x % kArraySize);
}
stop = performance.now();
print("arguments_array (fast aliased): " + (Math.floor((stop - start)*10)/10) + " ms");

delete arguments_array[5];
start = performance.now();
for (let x = 0; x < kIterationShort; ++x) {
  Array.prototype.slice.call(arguments_array, x % kArraySize);
}
stop = performance.now();
print("arguments_array (slow aliased): " + (Math.floor((stop - start)*10)/10) + " ms");
