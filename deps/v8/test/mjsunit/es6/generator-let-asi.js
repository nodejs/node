// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var let = 0;
function* generator() {
  let
  yield 0;
}

let it = generator();
let {value, done} = it.next();
assertEquals(0, value);
assertEquals(false, done);
