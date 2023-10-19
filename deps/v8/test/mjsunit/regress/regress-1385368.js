// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-temporal
const o1 = {valueOf:1337};
const o2 = Array(BigInt64Array,o1,..."short",o1);
const o3 = {__proto__:13.37,c:o2,e:"\u56FD\u52A1\u9662\u5173\u4E8E\u300A\u571F\u5730"};
const o4 = JSON.stringify(o3);
assertThrows('new Temporal.Calendar(o4);', RangeError);
