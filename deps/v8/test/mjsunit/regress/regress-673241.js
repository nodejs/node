// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm

function generateAsmJs() {
  'use asm';
  function fun() { fun(); }
  return fun;
}

assertThrows(generateAsmJs());
