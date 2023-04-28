// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() { return arguments }

// Reconfiguring function.name should update both the attributes and the value.
Object.defineProperty(f, "name", {
  writable: true, configurable: true, value: 10});
assertEquals({value: 10, writable: true, enumerable: false, configurable: true},
             Object.getOwnPropertyDescriptor(f, "name"));

var args = f();

// Setting a value for arguments[Symbol.iterator] should not affect the
// attributes.
args[Symbol.iterator] = 10;
assertEquals({value: 10, writable: true, configurable: true, enumerable: false},
             Object.getOwnPropertyDescriptor(args, Symbol.iterator));
