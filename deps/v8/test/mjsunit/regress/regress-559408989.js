// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Foo() {}
// Deleting Foo.name causes constructor debug name lookup to fall back to
// searching the prototype chain via GetDataProperty().
delete Foo.name;
// Bound functions implement "name" via a lazy AccessorInfo getter
// (BoundFunctionNameGetter) with SideEffectType::kHasNoSideEffect that
// allocates a string on the heap ("bound bar").
Object.setPrototypeOf(Foo, (function bar() {}).bind());

const obj = new Foo();
obj.self = obj;

// JSON.stringify should fail with a TypeError due to the circular structure.
// Under DisallowGarbageCollection, GetDataProperty() must not execute
// accessors that allocate memory.
const expectedMessage =
    "Converting circular structure to JSON\n" +
    "    --> starting at object with constructor 'Foo'\n" +
    "    --- property 'self' closes the circle";
assertThrows(() => JSON.stringify(obj), TypeError, expectedMessage);
