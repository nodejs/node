// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle-reprl --allow-natives-syntax

// JS_BUNDLE_SCRIPT
print("part 1 before:", %PromiseSpeciesProtector());
class MyPromise extends Promise {}
Object.defineProperty(Promise, Symbol.species, {value: MyPromise});
print("part 1 after:", %PromiseSpeciesProtector());

// JS_BUNDLE_SCRIPT
print("part 2:", %PromiseSpeciesProtector());
