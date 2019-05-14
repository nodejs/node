// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var a = Array(...Array(5)).map(() => 1);

  assertEquals([1, 1, 1, 1, 1], a);
})();
