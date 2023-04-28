// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = {foo: 0, 0: 0, 2: 2, 3: 3};
o.__defineSetter__("1", function(v) { this.foo = 0.1; });

for(var i = 0; i < 4; i++) {
  switch (i) {
    case 0: o.p1 = 0; break;
    case 1: o.p2 = 0; break;
  }
  o[i] = i;
}
