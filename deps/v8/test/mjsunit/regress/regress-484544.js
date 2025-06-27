// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-compaction --predictable

function f() {
  return [[], [], [[], [], []]];
}

for (var i=0; i<10000; i++) {
  f();
}
