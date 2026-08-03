// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(function() {
  const p = new Proxy({}, {});
  (new Set).add(p);  // Compute the hash code for p.
  null[p] = 0;
});
