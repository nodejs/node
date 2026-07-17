// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sandbox-testing

function f() { return 42; }
f();

const bytecode_obj = %GetBytecode(f);
bytecode_obj.bytecode = new ArrayBuffer(0);

%InstallBytecode(f, bytecode_obj);

assertUnreachable("Process should have been killed.");
