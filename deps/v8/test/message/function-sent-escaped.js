// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-function-sent

function* f() {
  return function.s\u0065nt;
}
for (var i of f()) print(i);
