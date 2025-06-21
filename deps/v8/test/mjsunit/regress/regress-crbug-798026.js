// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc

array = new Array(4 * 1024 * 1024);
Set.prototype.add = value => {
  if (array.length != 1) {
    array.length = 1;
    gc();
  }
}
new Set(array);
