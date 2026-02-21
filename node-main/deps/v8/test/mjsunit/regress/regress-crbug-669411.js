// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(o) {
  o[5000000] = 0;
}
var o = Object.create(null);
f(o);
f(o);
f(o);
