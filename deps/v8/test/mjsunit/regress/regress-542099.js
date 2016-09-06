// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-restrictive-declarations

// Previously, this caused a CHECK fail in debug mode
// https://code.google.com/p/chromium/issues/detail?id=542099

var foo = {};
var bar = foo;
for (foo.x in {a: 1}) function foo() { return foo; }
assertEquals("object", typeof bar);
assertEquals("a", bar.x);
assertEquals("function", typeof foo);
assertEquals("function", typeof foo());
assertSame(foo, foo());
assertEquals(undefined, foo.x);
