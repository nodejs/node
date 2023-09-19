// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

global = 0;

(function foo() {
  function bar() {
    let context_allocated = 0;
    with ({}) {
      f = function() { ++global; }
    }
    function baz() { return foo(context_allocated); };
    f();
  };
  bar();
})();
