// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --no-verify-get-js-builtin-state

const builtin_names = Sandbox.getBuiltinNames();

var f0 = BigInt;  // constructor with kDontAdaptArgumentsSentinel.

// ConstructFunction is used as a dummy (intentionally classified as
// DisabledFlagDependent). See Builtins::GetJSBuiltinState().
Sandbox.setFunctionCodeToBuiltin(
    f0, builtin_names.indexOf('ConstructFunction'));
assertUnreachable();
