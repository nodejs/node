// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Demonstrates spec violation: await incorrectly allowed as identifier
// and return incorrectly allowed in class static blocks when preceded
// by instance field members.

// Bug: await incorrectly allowed when instance field precedes static block
assertThrows(() => {
  eval('class C2 { x = 1; static { let await = 42; } }');
}, SyntaxError);

// Bug: multiple instance fields before static block
assertThrows(() => {
  eval('class C3 { a = 1; b = 2; static { let await = 99; } }');
}, SyntaxError);

// Bug: return incorrectly allowed when instance field precedes static block
assertThrows(() => {
  eval('class C5 { x = 1; static { return; } }');
}, SyntaxError);
