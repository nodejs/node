// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

__v_1 = new Uint8Array();
Object.defineProperty(__v_1.__proto__, 'length', {value: 42});
Array.prototype.includes.call(new Uint8Array(), 2);
