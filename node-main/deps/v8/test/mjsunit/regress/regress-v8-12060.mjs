// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-fuzzy-module-file-extensions

const paths = [
  ".",
  "..",
  "...",
  "/",
  "/..",
  "/../..",
  "../..",
  "./..",
  "./../",
  "./../..",
];

const results = await Promise.allSettled(
    paths.map(path => import(path + "/___._._")));
for (let result of results) {
  assertEquals(result.status, 'rejected');
}
