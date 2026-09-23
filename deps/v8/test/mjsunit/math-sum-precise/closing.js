// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise

(function() {
  function test(obj) {
    let it = obj.values();
    var closed = false;
    it.return = function () {
      closed = true;
      assertEquals("pos", it.next());
    };
    assertThrows(() => Math.sumPrecise(it), TypeError);
    assertTrue(closed);
  }
  test([1,"pos",2]);
  test([1,,"pos",2]);
  test([,1,"pos",2]);
  test([,"pos",]);
  test(new Set([1, "pos", 2]));
  let set = new Set([1,"pos"]);
  set.delete(1);
  test(set);
})();
