// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-modules --expose-debug-as=debug

(function () {  // Scope for utility functions.
  escaping_function = function(object) {
    // Argument must not be null or undefined.
    var string = Object.prototype.toString.call(object);
    // String has format [object <ClassName>].
    return string.substring(8, string.length - 1);
  }
})();

module B {
  var stuff = 3
}

var __v_0 = {};
var __v_4 = debug.MakeMirror(__v_0);
print(__v_4.referencedBy().length);  // core dump here if not fixed.
