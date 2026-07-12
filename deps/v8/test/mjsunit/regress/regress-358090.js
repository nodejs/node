// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var x = Array(100000);
y =  Array.apply(Array, x);
y.unshift(4);
y.shift();
