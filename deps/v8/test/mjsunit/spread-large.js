// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that spread can create arrays in large object space.

const n = 130000;

// Array
{
  let x = new Array(n);
  for (let i = 0; i < n; ++i) x[i] = i;
  let a = [...x];
}

// String
{
  let x = new Array(n);
  for (let i = 0; i < n; ++i) x[i] = i;
  let a = [...String(x)];
}

// Set
{
  let x = new Set();
  for (let i = 0; i < n; ++i) x.add(i);
  let a = [...x];
}{
  let x = new Set();
  for (let i = 0; i < n; ++i) x.add(i);
  let a = [...x.values()];
}{
  let x = new Set();
  for (let i = 0; i < n; ++i) x.add(i);
  let a = [...x.keys()];
}

// Map
{
  let x = new Map();
  for (let i = 0; i < n; ++i) x.set(i, String(i));
  let a = [...x.values()];
}{
  let x = new Map();
  for (let i = 0; i < n; ++i) x.set(i, String(i));
  let a = [...x.keys()];
}
