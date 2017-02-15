// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var b = { toString: function() { return "b"; } };
var c = { toString: function() { return "c"; } };

(function() {
  var expected_receiver;
  var obj1 = {
    a: 100,
    b_: 200,
    get b() { assertEquals(expected_receiver, this); return this.b_; },
    set b(v) { assertEquals(expected_receiver, this); this.b_ = v; },
    c_: 300,
    get c() { assertEquals(expected_receiver, this); return this.c_; },
    set c(v) { assertEquals(expected_receiver, this); this.c_ = v; },
  };
  var obj2 = {
    boom() {
      super.a++;
      super[b]++;
      super[c]++;
    },
  }
  Object.setPrototypeOf(obj2, obj1);

  expected_receiver = obj2;
  obj2.boom();
  assertEquals(101, obj2.a);
  assertEquals(201, obj2[b]);
  assertEquals(301, obj2[c]);

  expected_receiver = obj1;
  assertEquals(100, obj1.a);
  assertEquals(200, obj1[b]);
  assertEquals(300, obj1[c]);
}());
