// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure passing 1 or false to patched construtor won't cause crash

Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, { value: _ => true });
assertThrows(() =>
     Intl.NumberFormat.call(1), TypeError);

assertThrows(() =>
     Intl.NumberFormat.call(false), TypeError);
