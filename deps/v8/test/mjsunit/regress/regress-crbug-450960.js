// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=70

"a".replace(/a/g, "");

function test() {
   try {
     test();
   } catch(e) {
     "b".replace(/(b)/g, new []);
   }
}

try {
  test();
} catch (e) {
}
