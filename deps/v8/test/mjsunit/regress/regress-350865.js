// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-compaction --stack-size=150

/\2/.test("1");

function rec() {
  try {
    rec();
  } catch(e) {
    /\2/.test("1");
  }
}

rec();
