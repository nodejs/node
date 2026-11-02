// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let arr = [];
Object.setPrototypeOf(arr, Object.prototype);
assertEquals('{"\u1234":[]}', JSON.stringify({"\u1234": arr}));
assertEquals('{"a":[]}', JSON.stringify({"a": arr}));

let fn = function () {};
Object.setPrototypeOf(fn, Object.prototype);
assertEquals('{}', JSON.stringify({"\u1234": fn}));

let bf = (function () {}).bind(null);
Object.setPrototypeOf(bf, Object.prototype);
assertEquals('{}', JSON.stringify({"\u1234": bf}));

assertEquals('{}', JSON.stringify({"\u1234": Function.prototype}));
assertEquals('{}', JSON.stringify({"a": Function.prototype}));
