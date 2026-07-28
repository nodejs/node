// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const o1 = {Text: 'foo'};
const o2 = {Text: 'bar'};
Object.defineProperty(o2, Symbol.toPrimitive, {});
o2.toJSON = 42;
JSON.parse(JSON.stringify([o1,o2]));
