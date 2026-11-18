// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing

let id = Sandbox.getBuiltinNames().indexOf('ArrayMap');
let code = Sandbox.getObjectAt(Sandbox.getBuiltinCode(id));

assertThrows(() => {
  Sandbox.markForCorruptionOnAccess(code, 0);
}, TypeError, 'First argument must not be a read-only object');
