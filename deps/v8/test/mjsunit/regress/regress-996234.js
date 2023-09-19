// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --regexp-tier-up --print-code --trace-regexp-bytecodes

// Test printing of regexp code and bytecode when tiering up from the
// interpreter to the compiler.
function __f_13544(__v_62631) {
  __v_62631.replace(/\s/g);
}

__f_13544("No");

let re = /^.$/;
re.test("a");
re.test("3");
re.test("π");
