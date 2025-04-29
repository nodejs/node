// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let v1 = 0;

if (Math.random() > 0.5) {
  let v2 = "";
  v1++;
  if (v1) {
    const v3 = 0;
    v2 = v3 + "";
    v2 = v3 + "";
    v2 = v3 + "";
    v1 = v2;
    v1 = v2;
    v1 = v2;
  }
}
