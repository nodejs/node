// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-builtin-subclassing

class SubArray extends Array { }

// Constructor methods
assertInstanceof(SubArray.from([1,2,3]), Array);
assertEquals(SubArray.from([1,2,3]).constructor, Array);

assertInstanceof(SubArray.of([1,2,3]), Array);
assertEquals(SubArray.of([1,2,3]).constructor, Array);
