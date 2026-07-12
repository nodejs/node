// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var s = "f";

  // 2^18 length, enough to ensure an array (of pointers) bigger than 500KB.
  for (var i = 0; i < 18; i++) {
    s += s;
  }

  var ss = [...s];
})();
