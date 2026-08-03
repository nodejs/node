// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var arr = [];
for (var i = 0; i < 100000; i++) arr[i] = 0;
var fromIndex = {valueOf: function() { arr.length = 0; }};
arr.indexOf(1, fromIndex);
