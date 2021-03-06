// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-slow-asserts

var arr = [];
var str = new String('x');

function f(a,b) {
  a[b] = 1;
}

f(arr, 0);
f(str, 0);
f(str, 0);

// This is just to trigger elements validation, object already broken.
%SetKeyedProperty(str, 1, 'y');
