// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=100

(function() {
  var source = "[]"
  for (var i = 0; i < 300; i++) {
    source += ".concat([";
    for (var j = 0; j < 1000; j++) {
      source += "0,";
    }
    source += "0])"
  }
  eval(source);
})();
