// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
'use strict';

function test() {
  for (let x = void 0 of [1, 2, 3]) {
    return x;
  }
}
