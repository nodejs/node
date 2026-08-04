// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --interpreted-frames-native-stack

// Make sure that the interpreted trampoline copy (for native interpreter frames
// in stack traces) works for interperted functions but doesn't crash for asm.js

function func() {
  return;
}

function asm_func() {
  "use asm";
  function f(){}
  return {f:f};
}

function failed_asm_func() {
  "use asm";
  // This should fail validation
  [x,y,z] = [1,2,3];
  return;
}

func();
asm_func();
failed_asm_func();
