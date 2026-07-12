// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sandbox-testing

function f() { return 42; }
f();

const bytecode_obj = %GetBytecode(f);
const invalid_bytecode = new Uint8Array(1);
invalid_bytecode[0] = 0xFF;
bytecode_obj.bytecode = invalid_bytecode.buffer;

%InstallBytecode(f, bytecode_obj);

assertUnreachable("Process should have been killed.");
