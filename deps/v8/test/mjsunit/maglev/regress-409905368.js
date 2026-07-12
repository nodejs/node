// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

for (let v0 = 0; v0 < 2500; v0++) {
  const v2 = new Uint16Array(Uint16Array);
  v2[2] = v2;
}
