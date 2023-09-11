// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __isPropertyOfType() {
}
function __getProperties(obj) {
  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
 properties.push(name);
  }
  return properties;
}
function __getRandomProperty(obj, seed) {
  let properties = __getProperties(obj);
  return properties[seed % properties.length];
}
const __v_12 =
[ 2, '3'];
function __f_8() {
  if (__v_12 != null && typeof __v_12 == "object") Object.defineProperty(__v_12, __getRandomProperty(__v_12, 416937), {
    value: 4294967295
  });
}
  __f_8();
  var __v_15 = Object.freeze(__v_12);
  __f_8();
