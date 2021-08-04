// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

assertTrue(%TypedArraySpeciesProtector());
class MyUint32Array extends Uint32Array { }
Object.defineProperty(Uint32Array, Symbol.species, { value: MyUint32Array });
assertFalse(%TypedArraySpeciesProtector());
