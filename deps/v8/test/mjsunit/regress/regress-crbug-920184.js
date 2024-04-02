// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --enable-slow-asserts

var Ctor = function() {
  return [];
};
var a = ["one", "two", "three"];
a.constructor = {};
a.constructor[Symbol.species] = Ctor;

a.filter(function() { return true; });
