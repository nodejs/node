// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

(function () {
  var GeneratorFunction = function* () {}.constructor;
  class MyFunc extends GeneratorFunction {
    constructor(...args) {
      super(...args);
      this.o = {};
      this.o = {};
    }
  }
  var f = new MyFunc("'use strict'; yield 153;");
  gc();
  var f = new MyFunc("'use strict'; yield 153;");
})();
