// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var p = new Proxy({}, {
  has: function () { throw "nope"; }
});
p.length = 2;
assertThrows(() => Array.prototype.indexOf.call(p));
