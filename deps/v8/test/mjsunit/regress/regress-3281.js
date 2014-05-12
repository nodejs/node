// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-collections

// Should not crash or raise an exception.

var s = new Set();
var setIterator = %SetCreateIterator(s, 2);

var m = new Map();
var mapIterator = %MapCreateIterator(m, 2);
