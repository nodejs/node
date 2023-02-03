// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct

'use strict';

let fieldNames = [];
for (let i = 0; i < 999; i++) {
  fieldNames.push('field' + i);
}
let s = new (new SharedStructType(fieldNames));

// Write to an out-of-object field.
(function storeOutOfObject() {
  for (let i = 0; i < 100; i++) s.field998 = i;
})();

// Write to an in-object field.
(function storeInObject() {
  for (let i = 0; i < 100; i++) s.field0 = i;
})();
