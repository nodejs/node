// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  let x = [0,0,0,0,0];
  Object.defineProperty(x, 'length', {value : 4, enumerable : true});
}

assertThrows(f, TypeError);
