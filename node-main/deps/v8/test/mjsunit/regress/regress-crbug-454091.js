// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.__proto__ = Array.prototype;
Object.freeze(this);
this.length = 1;
assertThrows('this.__proto__ = {}');
assertEquals(Array.prototype, this.__proto__);
