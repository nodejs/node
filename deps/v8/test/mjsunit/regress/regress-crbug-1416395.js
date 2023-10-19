// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = /(?=.)/giv;
re.lastIndex = 4;
const str = 'f\uD83D\uDCA9ba\u2603';
let result = re[Symbol.split](str, 3);
assertEquals(['f','\uD83D\uDCA9','b'], result);
