// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let str = "";

// Many named captures force the resulting named capture backing store into
// large object space.
for (var i = 0; i < 0x2000; i++) str += "(?<a"+i+">)|";
str += "(?<b>)";

const regexp = new RegExp(str);
const result = "xxx".match(regexp);

assertNotNull(result);
