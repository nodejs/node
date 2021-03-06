// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt

for (let i = 0; i < 5000; i++) {
  try {
    [].reduce(function() {});
  } catch (x) {
  }
}
