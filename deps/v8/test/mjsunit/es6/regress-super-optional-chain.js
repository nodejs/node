// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Optional chaining on `super` should report `kOptionalChainingNoSuper`.

assertThrows(
    () => eval("class A { m() { return super?.x; } }"),
    SyntaxError,
    "Invalid optional chain from super property");
assertThrows(
    () => eval("class A { m() { return super?.['x']; } }"),
    SyntaxError,
    "Invalid optional chain from super property");
assertThrows(
    () => eval("class A extends Object { constructor() { super?.(); } }"),
    SyntaxError,
    "Invalid optional chain from super property");
