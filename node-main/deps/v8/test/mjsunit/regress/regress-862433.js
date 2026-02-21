// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var arr = [];
for (var i = 1; i != 390000; ++i) {
  arr.push("f()");
}
new Function(arr.join());
