//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Compares the value set by interpreter with the jitted code
// need to run with -mic:1 -off:simplejit -off:JITLoopBody
// Run locally with -trace:memop -trace:bailout to help find bugs

function test() {
  let n = 300;
  let i1 = 0, i2 = 5, i3 = -7, f1 = 3.14, f2 = -9.54715685478;
  let j1 = -50, j2 = 51, j3 = 100, j4 = 200, j5 = 275;
  let i;
  let a1 = new Int8Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a1[i] = i1;}
  for(i = j3; i < j4; i++)         {a1[i] = i3;}
  i = j2; while(i < j3)            {a1[i] = i2; ++i; a1[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a1[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a1[i] = f2;}

  let a2 = new Uint8Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a2[i] = i1;}
  for(i = j3; i < j4; i++)         {a2[i] = i3;}
  i = j2; while(i < j3)            {a2[i] = i2; ++i; a2[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a2[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a2[i] = f2;}

  let a3 = new Int16Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a3[i] = i1;}
  for(i = j3; i < j4; i++)         {a3[i] = i3;}
  i = j2; while(i < j3)            {a3[i] = i2; ++i; a3[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a3[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a3[i] = f2;}

  let a4 = new Uint16Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a4[i] = i1;}
  for(i = j3; i < j4; i++)         {a4[i] = i3;}
  i = j2; while(i < j3)            {a4[i] = i2; ++i; a4[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a4[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a4[i] = f2;}

  let a5 = new Int32Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a5[i] = i1;}
  for(i = j3; i < j4; i++)         {a5[i] = i3;}
  i = j2; while(i < j3)            {a5[i] = i2; ++i; a5[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a5[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a5[i] = f2;}

  let a6 = new Uint32Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a6[i] = i1;}
  for(i = j3; i < j4; i++)         {a6[i] = i3;}
  i = j2; while(i < j3)            {a6[i] = i2; ++i; a6[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a6[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a6[i] = f2;}

  let a7 = new Float32Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a7[i] = i1;}
  for(i = j3; i < j4; i++)         {a7[i] = i3;}
  i = j2; while(i < j3)            {a7[i] = i2; ++i; a7[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a7[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a7[i] = f2;}

  let a8 = new Float64Array(n);
  for(i = j1; i <= j2 - 1; i++)    {a8[i] = i1;}
  for(i = j3; i < j4; i++)         {a8[i] = i3;}
  i = j2; while(i < j3)            {a8[i] = i2; ++i; a8[i] = i2; ++i;}
  for(i = j5 - 1; i > j4 + 1; i--) {a8[i] = f1;}
  for(i = n - 1; i >= j5; i--)     {a8[i] = f2;}

  return [a1, a2, a3, a4, a5, a6, a7, a8];
}

// Run first time in interpreter
let a = test();
// Run second time with memop
let b = test();

let passed = true;
for(let i = 0; i < a.length; i++) {
  let aa = a[i], bb = b[i];
  for(let j = 0; j < aa.length; j++) {
    if(aa[j] !== bb[j]) {
      WScript.Echo(types[i] + " " + j + " " + aa[j] + " " + bb[j]);
      passed = false;
    }
  }
}

if(passed) {
  WScript.Echo("PASSED");
} else {
  WScript.Echo("FAILED");
}
