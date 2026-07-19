// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function baz() {
  "use asm";
}
function B(stdlib, env) {
  "use asm";
  var x = env.foo | 0;
}
var bar = {
  get foo() {
  }
};
bar.__defineGetter__('foo', function() { return baz(); });
B(this, bar);
