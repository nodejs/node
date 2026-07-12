// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (let i = 0; i < 3; i++) {
  for (let j = 0; j < 32767; j++) Number;
  for (let j = 0; j < 2335; j++) Number;
  var arr = [, ...(new Int16Array(0xffff)), 0.5];
  arr.concat(Number, arr)
}
