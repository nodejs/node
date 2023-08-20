// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

var re = /\w/;
re.test("a");  // Trigger regexp compile.

function rec() {
  try {
    return rec();
  } catch (e) {
    return re.test("b");
  }
}

assertTrue(rec());
