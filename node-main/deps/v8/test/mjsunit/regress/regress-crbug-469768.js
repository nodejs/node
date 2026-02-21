// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Try several different argument counts to make sure none of them
// sneak through the system of stack checks.

try {
  Array.prototype.concat.apply([], new Array(100000));
} catch (e) {
  // Throwing is fine, just don't crash.
}


try {
  Array.prototype.concat.apply([], new Array(150000));
} catch (e) {
  // Throwing is fine, just don't crash.
}


try {
  Array.prototype.concat.apply([], new Array(200000));
} catch (e) {
  // Throwing is fine, just don't crash.
}


try {
  Array.prototype.concat.apply([], new Array(248000));
} catch (e) {
  // Throwing is fine, just don't crash.
}
