//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Compares the value set by interpreter with the jitted code
// need to run with -mic:1 -off:simplejit -off:JITLoopBody
// Run locally with -trace:memop -trace:bailout to help find bugs

const global = this;
const types = "Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array".split(" ");
const n = 500;

const srcs = [], dsts = [];
for(let type of types) {
  const src = new global[type](n);
  const dst = new global[type](n);
  for(let i = 0; i < n; ++i) {
    src[i] = i + 0.5;
    dst[i] = 0;
  }
  srcs.push(src);
  dsts.push(dst);
}

// Can't use the same function twice or it will bailout
function test0(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test1(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test2(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test3(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test4(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test5(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test6(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }
function test7(a, b, start) { for (let i = start; i < start + (n / 2); i++) { b[i] = a[i]; } }

test0(srcs[0], dsts[0], 0);
test0(srcs[0], dsts[0], n / 2);
test1(srcs[1], dsts[1], 0);
test1(srcs[1], dsts[1], n / 2);
test2(srcs[2], dsts[2], 0);
test2(srcs[2], dsts[2], n / 2);
test3(srcs[3], dsts[3], 0);
test3(srcs[3], dsts[3], n / 2);
test4(srcs[4], dsts[4], 0);
test4(srcs[4], dsts[4], n / 2);
test5(srcs[5], dsts[5], 0);
test5(srcs[5], dsts[5], n / 2);
test6(srcs[6], dsts[6], 0);
test6(srcs[6], dsts[6], n / 2);
test7(srcs[7], dsts[7], 0);
test7(srcs[7], dsts[7], n / 2);

let passed = 1;

for(let i in types) {
  const src = srcs[i], dst = dsts[i];
  for(let j = 0; j < n; j++) {
    if(src[j] !== dst[j])
    {
      passed = 0;
      WScript.Echo(types[i] + " " + j + " " + src[j] + " " + dst[j]);
      break;
    }
  }
}

if(passed === 1) {
  WScript.Echo("PASSED");
} else {
  WScript.Echo("FAILED");
}
