// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --ignition-filter=-foo

function* foo() { yield 42 }
function* goo() { yield 42 }
var f = foo();
var g = goo();
assertEquals(42, f.next().value);
assertEquals(42, g.next().value);
assertEquals(true, f.next().done);
assertEquals(true, g.next().done);
