// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%TypedArraySpeciesProtector());
class MyBigUint64Array extends BigUint64Array { }
Object.defineProperty(BigUint64Array, Symbol.species, { value: MyBigUint64Array });
assertFalse(%TypedArraySpeciesProtector());
