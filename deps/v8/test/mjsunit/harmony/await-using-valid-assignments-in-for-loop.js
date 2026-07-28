// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function TestCStyleForAssignment() {
  for (await using x = {value: 42, [Symbol.asyncDispose]() {}}; x.value < 45;
       x.value++) {
    x.value++;
  }

  for (const y = {
         value: 42,
       };
       y.value < 45; y.value++) {
    y.value++;
  }
}

async function RunTest() {
  await TestCStyleForAssignment();
}

RunTest();
