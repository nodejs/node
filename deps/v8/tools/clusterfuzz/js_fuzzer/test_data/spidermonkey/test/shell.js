// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('/test/shell.js');

function testFun(test) {
  if (test) throw Exception();
}

testFun(() => { throw Exception(() => { throw Exception()}); });
