// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

const l = 1000000000;
const a = [];
function foo() { var x = new Int32Array(l); }
try { foo(); } catch (e) { }
