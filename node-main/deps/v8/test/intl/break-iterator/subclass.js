// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var locales = ["tlh", "id", "en"];
var input = "foo and bar";
var refBreakIterator = new Intl.v8BreakIterator(locales);
refBreakIterator.adoptText(input);

class MyBreakIterator extends Intl.v8BreakIterator {
  constructor(locales, options) {
    super(locales, options);
  }
}

var myBreakIterator = new MyBreakIterator(locales);
myBreakIterator.adoptText(input);

let expectedPos = refBreakIterator.first();
let actualPos = myBreakIterator.first();
assertEquals(expectedPos, actualPos);

while (expectedPos != -1) {
  expectedPos = refBreakIterator.next();
  actualPos = myBreakIterator.next();
  assertEquals(expectedPos, actualPos);
}
