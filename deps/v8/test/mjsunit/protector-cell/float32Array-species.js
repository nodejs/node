// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

assertTrue(%TypedArraySpeciesProtector());
class MyFloat32Array extends Float32Array { }
Object.defineProperty(Float32Array, Symbol.species, { value: MyFloat32Array });
assertFalse(%TypedArraySpeciesProtector());
