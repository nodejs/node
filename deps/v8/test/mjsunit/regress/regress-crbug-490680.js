// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sentinel = null;

try {
  throw { toString: function() { sentinel = "observed"; } };
} catch (e) {
}

L: try {
  throw { toString: function() { sentinel = "observed"; } };
} finally {
  break L;
}

assertNull(sentinel);
