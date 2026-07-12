// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const obj = {
    [Symbol.replace]() {},
    a: '\u{D83D}\u{DE0A}'
};
const arr = [obj, obj];
assertEquals('[{"a":"😊"},{"a":"😊"}]', JSON.stringify(arr));
