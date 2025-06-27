// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%TypedArraySpeciesProtector());
class MyUint8Array extends Uint8Array { }
Object.defineProperty(Uint8Array, Symbol.species, { value: MyUint8Array });
assertFalse(%TypedArraySpeciesProtector());
