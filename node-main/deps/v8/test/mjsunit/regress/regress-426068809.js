// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  let it = "x".matchAll();
  return it.next();
}

RegExp.prototype.exec = function(str) {
  this.lastIndex = 0x7FFFFFF0;
  return [""];
};

foo();
