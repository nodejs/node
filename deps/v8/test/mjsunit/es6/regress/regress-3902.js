// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function* g() {}
assertTrue(Object.getOwnPropertyDescriptor(g.__proto__, "constructor").configurable);
assertTrue(Object.getOwnPropertyDescriptor(g.prototype.__proto__, "constructor").configurable);

function FakeGeneratorFunctionConstructor() {}
Object.defineProperty(g.__proto__, "constructor", {value: FakeGeneratorFunctionConstructor});
assertSame(g.__proto__.constructor, FakeGeneratorFunctionConstructor);

function FakeGeneratorObjectConstructor() {}
Object.defineProperty(g.prototype.__proto__, "constructor", {value: FakeGeneratorObjectConstructor});
assertSame(g.prototype.__proto__.constructor, FakeGeneratorObjectConstructor);
