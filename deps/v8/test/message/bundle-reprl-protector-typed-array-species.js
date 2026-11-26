// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle-reprl --allow-natives-syntax

// JS_BUNDLE_SCRIPT
print("part 1 before:", %TypedArraySpeciesProtector());
class MyUint8Array extends Uint8Array {}
Object.defineProperty(Uint8Array, Symbol.species, {value: MyUint8Array});
print("part 1 after:", %TypedArraySpeciesProtector());

// JS_BUNDLE_SCRIPT
print("part 2:", %TypedArraySpeciesProtector());
