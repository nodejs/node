// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NumberToString() {
  var ret;
  var num = 10240;
  var obj = {};

  for ( var i = 0; i < num; i++ ) {
    ret = obj["test" + num];
  }
}

createSuite('NumberToString', 1000, NumberToString);
