// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var log = [];

function f() {}
Object.defineProperty(Function.prototype, "name", {
  get() { log.push("getter"); return "ok"; }
});
delete f.name;
var b = f.bind();
assertEquals("bound ok", b.name);
assertEquals("bound ok", b.name);
assertEquals("bound ok", b.name);
assertEquals(["getter"], log);
