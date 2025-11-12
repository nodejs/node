// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

Debug = debug.Debug

function overflow() {
  return overflow();
}

Debug.setBreakOnException();
assertThrows(overflow, RangeError);
