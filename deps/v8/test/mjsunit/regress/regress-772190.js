// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

assertThrows(function() {
  __v_13383[4];
  let __v_13383 = {};
});
