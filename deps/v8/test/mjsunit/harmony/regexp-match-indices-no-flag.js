// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-regexp-match-indices

// Redefined hasIndices should not reflect in flags without
// --harmony-regexp-match-indices
{
  let re = /./;
  Object.defineProperty(re, "hasIndices", { get: function() { return true; } });
  assertEquals("", re.flags);
}
