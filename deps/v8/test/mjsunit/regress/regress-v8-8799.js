// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --flush-bytecode --stress-flush-code

// Ensure tagged template objects are cached even after bytecode flushing.
var f = (x) => eval`a${x}b`;
var a = f();
gc();
assertSame(a, f());
