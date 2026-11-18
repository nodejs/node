// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints

let arr = new Array(200).fill(1);
let elements = Sandbox.dereferenceTaggedPointerField(arr, 'elements');
// Watch the unused tail slot after popping one element so that falling back to
// MoveElements during arr.shift() does not read and corrupt the watched slot.
arr.pop();
Sandbox.markForCorruptionOnAccess(elements, 804);
arr.shift();
Sandbox.markForCorruptionOnAccess({a: 1}, 0);
