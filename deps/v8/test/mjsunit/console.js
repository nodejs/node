// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.assert(true);
console.assert("yes");
assertThrows(() => console.assert(false), Error);
assertThrows(() => console.assert(""), Error);
assertThrows(() => console.assert(0), Error);

let args = ["", {}, [], this, Array, 1, 1.4, true, false];

console.log(...args);
console.error(...args);
console.warn(...args);
console.info(...args);
console.debug(...args);

console.time();
console.timeEnd();

console.time("a");
console.timeEnd("a");

console.timeStamp();
args.forEach(each => console.timeStamp(each));

console.trace();
