// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f1() {
  let y = 200;
  try {
    throw {}
  } catch ({x=()=>y, y=300}) {
    return x()
  }
}
assertEquals(300, f1());

function f2() {
  let y = 200;
  try {
    throw {}
  } catch ({x=()=>y}) {
    let y = 300;
    return x()
  }
}
assertEquals(200, f2());
