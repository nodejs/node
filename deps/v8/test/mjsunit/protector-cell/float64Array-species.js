// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

assertTrue(%TypedArraySpeciesProtector());
class MyFloat64Array extends Float64Array { }
Object.defineProperty(Float64Array, Symbol.species, { value: MyFloat64Array });
assertFalse(%TypedArraySpeciesProtector());
