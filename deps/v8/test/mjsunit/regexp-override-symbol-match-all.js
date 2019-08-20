// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var s = "baa";

assertEquals([["b"]], [...s.matchAll(/./)]);

RegExp.prototype[Symbol.matchAll] = () => 42;
assertEquals(42, s.matchAll(/a./));
