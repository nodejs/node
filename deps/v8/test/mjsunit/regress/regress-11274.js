// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var typedArrayProto = Uint8Array.prototype.__proto__;
var typedArrayConstructor = Uint8Array.prototype.constructor[Symbol.species];
var called = false;

Uint8Array.prototype.__proto__ = {};
Uint8Array.prototype.constructor = {
    get [Symbol.species]() {
        called = true;
        return typedArrayConstructor;
    }
}

Uint8Array.prototype.__proto__ = typedArrayProto;
var arr = new Uint8Array(8);
arr.slice(1, 5);
assertTrue(called);
