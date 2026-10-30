// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let re = /\d/g;
const replacer = { toString() { re.lastIndex = 0; return ""; } };
function foo(subject) {
  re.lastIndex = -1;
  return subject.replace(re, replacer);
}
assertEquals('abcdefg', foo('abcdefg1234567'));
