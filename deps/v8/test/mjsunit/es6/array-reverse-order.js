// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ES6 specifically says that elements should be checked with [[HasElement]] before
// [[Get]]. This is observable in case a getter deletes elements. ES5 put the
// [[HasElement]] after the [[Get]].

assertTrue(1 in Array.prototype.reverse.call(
    {length:2, get 0(){delete this[0];}, 1: "b"}))
