// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let lf = new Intl.ListFormat("en");

// Test normal array
assertDoesNotThrow(() => lf.format(['a','b','c']));
assertThrows("lf.format(['a','b',3])",  TypeError, "Iterable yielded 3 which is not a string");

// Test sparse array
let sparse = ['a','b'];
sparse[10] = 'c';
assertThrows("lf.format(sparse)",  TypeError, "Iterable yielded undefined which is not a string");

// Test iterable of all String
let iterable_of_strings = {
  [Symbol.iterator]() {
    return this;
  },
  count: 0,
  next() {
    if (this.count++ < 4) {
      return {done: false, value: String(this.count)};
    }
    return {done:true}
  }
};
assertDoesNotThrow(() => lf.format(iterable_of_strings));

// Test iterable of none String throw TypeError
let iterable_of_strings_and_number = {
  [Symbol.iterator]() {
    return this;
  },
  count: 0,
  next() {
    this.count++;
    if (this.count ==  3) {
      return {done:false, value: 3};
    }
    if (this.count < 5) {
      return {done: false, value: String(this.count)};
    }
    return {done:true}
  }
};
assertThrows("lf.format(iterable_of_strings_and_number)",
    TypeError, "Iterable yielded 3 which is not a string");
assertEquals(3, iterable_of_strings_and_number.count);
