// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%TypedArraySpeciesProtector());
class MyInt8Array extends Int8Array { }
Object.defineProperty(Int8Array, Symbol.species, { value: MyInt8Array });
assertFalse(%TypedArraySpeciesProtector());
