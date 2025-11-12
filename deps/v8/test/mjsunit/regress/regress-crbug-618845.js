// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Foo() {}
Object.defineProperty(Foo.prototype, "name",
                      {get: function() { return "FooName"; }});

function ic(f) {
  return f.prototype.name;
}

assertEquals("FooName", ic(Foo));
assertEquals("FooName", ic(Foo));  // Don't crash, don't time out.
