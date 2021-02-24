// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

assertTrue(%TypedArraySpeciesProtector());
class MyInt16Array extends Int16Array { }
Object.defineProperty(Int16Array, Symbol.species, { value: MyInt16Array });
assertFalse(%TypedArraySpeciesProtector());
