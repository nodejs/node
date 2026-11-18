// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-force-slow-path

const inner = new Array(1e6).fill(1);
const outer = new Array(300).fill(inner);
assertThrows(() => outer.flat(), RangeError);
