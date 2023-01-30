// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --allow-natives-syntax --verify-heap --shared-string-table

const old = {};
old.bar = 100;

gc();
assertFalse(%InYoungGeneration(old));

const foo = %FlattenString('a'.repeat(512 * 1024));
assertTrue(%InYoungGeneration(foo));
assertTrue(%IsInPlaceInternalizableString(foo));

// Create old-to-new reference.
old.foo = foo;

gc();

assertTrue(!%InYoungGeneration(foo));
assertTrue(%InSharedHeap(foo));

// An additional full GC for heap verification.
gc();
