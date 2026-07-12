// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%TypedArraySpeciesProtector());
class MyUint16Array extends Uint16Array { }
Object.defineProperty(Uint16Array, Symbol.species, { value: MyUint16Array });
assertFalse(%TypedArraySpeciesProtector());
