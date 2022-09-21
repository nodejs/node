// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%ArraySpeciesProtector());
class MyArray extends Array { }
Object.defineProperty(Array, Symbol.species, { value: MyArray });
assertFalse(%ArraySpeciesProtector());

assertTrue(%PromiseSpeciesProtector());
class MyPromise extends Promise { }
Object.defineProperty(Promise, Symbol.species, { value: MyPromise });
assertFalse(%PromiseSpeciesProtector());

assertTrue(%RegExpSpeciesProtector());
class MyRegExp extends RegExp { }
Object.defineProperty(RegExp, Symbol.species, { value: MyRegExp });
assertFalse(%RegExpSpeciesProtector());
