// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals("function", (function f() { f = 42; return typeof f })());
assertEquals("function",
             (function* g() { g = 42; yield typeof g })().next().value);
