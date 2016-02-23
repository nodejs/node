// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-natives-as=builtins
// Should not crash or raise an exception.

var SetIterator = builtins.ImportNow("SetIterator");
var s = new Set();
var setIterator = new SetIterator(s, 2);

var MapIterator = builtins.ImportNow("MapIterator");
var m = new Map();
var mapIterator = new MapIterator(m, 2);
