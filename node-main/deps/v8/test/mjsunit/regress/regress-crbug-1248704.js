// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let evil = {
  valueOf: function () {
    array.length = 1;
  }
};
let array = [1, 2, 3];
let newArray = array.slice(evil);
assertEquals(3, newArray.length);
