// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


let arr = [...Array(9000)];
for (let j = 0; j < 40; j++) {
  Reflect.ownKeys(arr).shift();
  Array(64386);
}
