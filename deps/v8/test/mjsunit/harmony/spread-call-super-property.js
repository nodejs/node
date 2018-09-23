// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-spreadcalls --harmony-sloppy --harmony-rest-parameters

(function testCallSuperProperty() {
  class BaseClass {
    strict_method(...args) { "use strict"; return [this].concat(args); }
    sloppy_method(...args) { return [this].concat(args); }
  }
  class SubClass extends BaseClass {
    strict_m(...args) { return super.strict_method(...args); }
    sloppy_m(...args) { return super.sloppy_method(...args); }
  }

  var c = new SubClass();
  assertEquals([c, 1, 2, 3, 4, 5], c.strict_m(1, 2, 3, 4, 5));
  assertEquals([c, 1, 2, 3, 4, 5], c.sloppy_m(1, 2, 3, 4, 5));
})();
