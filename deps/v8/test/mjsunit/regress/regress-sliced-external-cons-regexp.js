// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --expose-gc

var re = /(B)/;
var cons1 = "0123456789" + "ABCDEFGHIJ";
var cons2 = "0123456789\u1234" + "ABCDEFGHIJ";
gc();
gc();  // Promote cons.

externalizeString(cons1);
externalizeString(cons2);

var slice1 = cons1.slice(1,-1);
var slice2 = cons2.slice(1,-1);
for (var i = 0; i < 10; i++) {
  assertEquals(["B", "B"], re.exec(slice1));
  assertEquals(["B", "B"], re.exec(slice2));
}
