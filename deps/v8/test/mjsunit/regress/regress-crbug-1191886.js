// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let arr = [];
for (var i = 0; i < 1000000; i++) {
  arr[i] = [];
}
assertEquals(1000000, i);
