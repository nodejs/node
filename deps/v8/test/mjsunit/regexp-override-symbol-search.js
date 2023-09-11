// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var s = "baa";

assertEquals(1, s.search(/a/));

RegExp.prototype[Symbol.search] = () => 42;
assertEquals(42, s.search(/a/));
