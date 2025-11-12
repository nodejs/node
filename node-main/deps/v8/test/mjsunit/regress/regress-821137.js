// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that creating an iterator that shrinks the array populated by
// Array.from does not lead to out of bounds writes.
let oobArray = [];
let maxSize = 1028 * 8;
Array.from.call(function() { return oobArray }, {[Symbol.iterator] : _ => (
  {
    counter : 0,
    next() {
      let result = this.counter++;
      if (this.counter > maxSize) {
        oobArray.length = 0;
        return {done: true};
      } else {
        return {value: result, done: false};
      }
    }
  }
) });
assertEquals(oobArray.length, maxSize);

// iterator reset the length to 0 just before returning done, so this will crash
// if the backing store was not resized correctly.
oobArray[oobArray.length - 1] = 0x41414141;
