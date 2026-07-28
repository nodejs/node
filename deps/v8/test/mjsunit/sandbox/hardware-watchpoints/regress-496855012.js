// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing

let str = 'abcde';
Sandbox.markForCorruptionOnAccess(str, 'length');
Object.getOwnPropertyDescriptor(str, 0);
