// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://ecma-international.org/ecma-262/6.0/#sec-json.stringify
// Step 4.b.iii.5.f.i

var log = [];

var replacer = Object.defineProperty([], 0, {
  get() {
    log.push('get 0');
  }
});
var space = Object.defineProperty(new String, 'toString', {
  value() {
    log.push('toString');
    return '';
  }
});

JSON.stringify('', replacer, space);
assertEquals(2, log.length);
assertEquals('get 0', log[0]);
assertEquals('toString', log[1]);
