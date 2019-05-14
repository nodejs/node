// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should not crash.
//
this.__defineGetter__(
  "x", (a = (function f() { return; (function() {}); })()) => { });
x;
