// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/1233397.
let y_power = 13311n * 64n; // Large enough to choose the Barrett algorithm.
// A couple of digits and a couple of intra-digit bits larger.
let x_power = y_power + 50n * 64n + 30n;
let x = 2n ** x_power;
let y = 2n ** y_power;
let q = x / y;
