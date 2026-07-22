// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var x = 1;
var g = eval("var y = 100; function h(s) { if (s) x = s; return x+y; }; h");

assertEquals(101, g());
assertEquals(102, g(2));
assertEquals(102, g(2));
