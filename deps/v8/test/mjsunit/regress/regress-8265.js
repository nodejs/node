// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (let i = 0; i < 54; ++i) Math.random();
let sum = 0;
for (let i = 0; i < 10; ++i)
  sum += Math.floor(Math.random() * 50);

assertNotEquals(0, sum);
