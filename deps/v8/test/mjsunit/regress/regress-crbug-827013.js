// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function Test() {
  var f = () => 42;
  delete f.length;
  delete f.name;

  var g = Object.create(f);
  for (var i = 0; i < 5; i++) {
    g.dummy;
  }
  assertTrue(%HasFastProperties(f));

  var h = f.bind(this);
})();
