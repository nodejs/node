// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Foo(do_throw) {
  if (do_throw) throw new Error("get me outta here");
}
var foo = new Foo(false);
(function caller() {
  Foo.call(foo, true);
})();
