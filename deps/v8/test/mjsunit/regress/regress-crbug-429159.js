// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  var src = "return " + Array(12000).join("src,") + "src";
  var fun = Function(src);
  assertEquals(src, fun());
} catch (e) {
  // Some architectures throw a RangeError, that is fine.
  assertInstanceof(e, RangeError);
}
