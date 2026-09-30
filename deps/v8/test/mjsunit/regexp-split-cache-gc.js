// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Avoid gc collecting the RegExpData, to preserve cache keys.
const re = / +/;

// This tests a new path into allocating fast C calls, via stub frames. A GC
// here scans the calling builtin's frame while its sp is unknown.
%SetAllocationTimeout(1, 1);
const parts = "the quick  brown   fox".split(re);
%SetAllocationTimeout(-1, -1);

assertEquals(["the", "quick", "brown", "fox"], parts);

// Hits the cache, reading the root as the lazy allocation left it rather than
// as it was deserialized.
assertEquals(["the", "quick", "brown", "fox"],
             "the quick  brown   fox".split(re));
