// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --no-verify-get-js-builtin-state

const builtin_names = Sandbox.getBuiltinNames();

var f0 = BigInt;  // constructor with kDontAdaptArgumentsSentinel.

Sandbox.setFunctionCodeToBuiltin(f0, builtin_names.indexOf("SharedStructTypeConstructor"));
// If this test starts to fail because we shipped --harmony-struct, the test
// must be updated to use a builtin belonging to some other not-yet shipped
// feature.
assertUnreachable();
