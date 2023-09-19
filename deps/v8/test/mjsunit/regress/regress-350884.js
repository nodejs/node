// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var obj = new Array(1);
obj[0] = 0;
obj[1] = 0;
function foo(flag_index) {
  obj[flag_index]++;
}

// Force dictionary properties on obj.
obj[-8] = 3;
foo(1);
foo(2);
