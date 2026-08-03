// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Need a fast array with enough elements to surpass
// kMaxRegularHeapObjectSize.
var size = 63392;
var a = [];
function build() {
  for (let i = 0; i < size; i++) {
    a.push(i);
  }
}

build();

function c(v) { return v + 0.5; }
a.map(c);
