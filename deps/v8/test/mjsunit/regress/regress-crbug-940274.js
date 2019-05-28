// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  var a = new Array({});
  a.shift();
  assertFalse(a.hasOwnProperty(0));
}

foo();
