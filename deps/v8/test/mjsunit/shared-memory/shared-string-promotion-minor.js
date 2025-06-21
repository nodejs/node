// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --allow-natives-syntax --verify-heap --shared-string-table

const old = {};
old.bar = 100;

gc();
assertFalse(%InYoungGeneration(old));

const foo = 'a'.repeat(9);
assertTrue(%InYoungGeneration(foo));
assertTrue(%IsInPlaceInternalizableString(foo));

// Create old-to-new reference.
old.foo = foo;

// The second minor GC would usally promote that string
// into old space, with --shared-string-table it is promoted
// into shared heap instead. This should create an old-to-shared
// reference from an old-to-new slot.
gc({type: "minor"});
gc({type: "minor"});

// An additional full GC for heap verification.
gc();
