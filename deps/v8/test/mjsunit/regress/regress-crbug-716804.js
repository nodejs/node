// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = [];
v.__proto__ = function() {};
v.prototype;

var v = [];
v.__proto__ = new Error();
v.stack;
