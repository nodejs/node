// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-experimental-regexp-engine-on-excessive-backtracks
// Flags: --regexp-backtracks-before-fallback=10
// Flags: --experimental-regexp-engine-capture-group-opt
// Flags: --experimental-regexp-engine-capture-group-opt-max-memory-usage=0

const re = /((a|b)(c|d)(e|f)(g|h))*i/;
assertThrows(() => re.exec("abcdefgh".repeat(100)));
assertThrows(() => re.exec("abcdefgh".repeat(100)));
