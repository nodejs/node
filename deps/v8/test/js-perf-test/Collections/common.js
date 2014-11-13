// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var N = 10;
var keys;


function SetupSmiKeys() {
  keys = new Array(N * 2);
  for (var i = 0; i < N * 2; i++) {
    keys[i] = i;
  }
}


function SetupStringKeys() {
  keys = new Array(N * 2);
  for (var i = 0; i < N * 2; i++) {
    keys[i] = 's' + i;
  }
}


function SetupObjectKeys() {
  keys = new Array(N * 2);
  for (var i = 0; i < N * 2; i++) {
    keys[i] = {};
  }
}
