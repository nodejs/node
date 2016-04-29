// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = [20, 21, 22, 23];
a.__proto__ = [10, 11, 12, 13];

var values = [];
var indices = [];
function callback(value, index, object) {
  object.length = 2;
  values.push(value);
  indices.push(index);
}
a.forEach(callback);
assertEquals([20, 21, 12, 13], values);
assertEquals([0, 1, 2, 3], indices);
