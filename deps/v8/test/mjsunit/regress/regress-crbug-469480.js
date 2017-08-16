// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-compaction --verify-heap
// Flags: --stack-size=100

// Load the debug context to fill up code space.
%GetDebugContext();
%GetDebugContext();

// Recurse and run regexp code.
assertThrows(function f() { f(/./.test("a")); }, RangeError);
