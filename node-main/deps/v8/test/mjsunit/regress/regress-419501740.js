// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class clazz {
};
const obj = [{[clazz]: -430.85078006555455 }, {foo: 168}];
const expected = '[{"class clazz {\\n}":-430.85078006555455},{"foo":168}]';
assertEquals(expected, (JSON.stringify(JSON.parse(JSON.stringify(obj)))));
