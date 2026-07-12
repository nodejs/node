// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let ar = new Int32Array();
assertThrows(()=> {
  Object.defineProperty(ar, 1073741824, { get: undefined });
}, TypeError);

assertThrows(()=> {
  Object.defineProperty(ar, 2147483648, { get: undefined });
}, TypeError);

assertThrows(()=> {
  Object.defineProperties(ar, { 1073741824: { get: undefined } });
}, TypeError);

assertThrows(()=> {
  Object.defineProperties(ar, { 2147483648: { get: undefined } });
}, TypeError);
