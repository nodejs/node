// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

assertTrue(%TypedArraySpeciesProtector());
class MyBigInt64Array extends BigInt64Array { }
Object.defineProperty(BigInt64Array, Symbol.species, { value: MyBigInt64Array });
assertFalse(%TypedArraySpeciesProtector());
