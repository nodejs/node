// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj1 = {
  foo: [],
  bar: 1
};
let obj2 = {
  foo: [{}],
  bar: 2
};
assertEquals('{"foo":[],"bar":1}', JSON.stringify(obj1));
assertEquals('{"foo":[{}],"bar":2}', JSON.stringify(obj2));
Object.defineProperty(obj2, undefined, { get: function () {}});
assertEquals('{"foo":[],"bar":1}', JSON.stringify(obj1));
assertEquals('{"foo":[{}],"bar":2}', JSON.stringify(obj2));
