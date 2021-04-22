// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-enable-experimental-regexp-engine

let badregexp =  "(?:" +  " ".repeat(32768*2)+  ")*";
reg = RegExp(badregexp);
assertThrows(() => reg.test(), SyntaxError);
