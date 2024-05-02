// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function boom() {
  var args = [];
  for (var i = 0; i < 110000; i++)
    args.push(1.1);
  return Array.apply(Array, args);
}
var array = boom();
