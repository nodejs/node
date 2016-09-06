// Copyright 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description('Tests for the descriptors of the properties implicitly defined by ES6 class syntax');

function descriptor(object, propertyName) {
    return Object.getOwnPropertyDescriptor(object, propertyName);
}

function enumeratedProperties(object) {
    var properties = [];
    for (var propertyName in object)
        properties.push(propertyName);
    return properties;
}

Array.prototype.includes = function(element) {
  return this.indexOf(element) !== -1;
};

shouldBeFalse('class A {}; descriptor(A, "prototype").writable');
shouldBe('class A {}; var x = A.prototype; A.prototype = 3; A.prototype', 'x');
shouldBeFalse('class A {}; descriptor(A, "prototype").enumerable');
shouldBeTrue('class A {}; A.foo = "foo"; enumeratedProperties(A).includes("foo")');
shouldBeFalse('class A {}; enumeratedProperties(A).includes("prototype")');
shouldBeFalse('class A {}; descriptor(A, "prototype").configurable');
shouldThrow('class A {}; Object.defineProperty(A, "prototype", {value: "foo"})', '"TypeError: Cannot redefine property: prototype"');

shouldBeTrue('class A { static foo() {} }; descriptor(A, "foo").writable');
shouldBe('class A { static foo() {} }; A.foo = 3; A.foo', '3');
shouldBeFalse('class A { static foo() {} }; descriptor(A, "foo").enumerable');
shouldBeFalse('class A { static foo() {} }; enumeratedProperties(A).includes("foo")');
shouldBeTrue('class A { static foo() {} }; descriptor(A, "foo").configurable');
shouldBe('class A { static foo() {} }; Object.defineProperty(A, "foo", {value: "bar"}); A.foo', '"bar"');

shouldBe('class A { static get foo() {} }; descriptor(A, "foo").writable', 'undefined');
shouldBe('class A { static get foo() { return 5; } }; A.foo = 3; A.foo', '5');
shouldBeFalse('class A { static get foo() {} }; descriptor(A, "foo").enumerable');
shouldBeFalse('class A { static get foo() {} }; enumeratedProperties(A).includes("foo")');
shouldBeFalse('class A { static get foo() {} }; enumeratedProperties(new A).includes("foo")');
shouldBeTrue('class A { static get foo() {} }; descriptor(A, "foo").configurable');
shouldBe('class A { static get foo() {} }; Object.defineProperty(A, "foo", {value: "bar"}); A.foo', '"bar"');

shouldBeTrue('class A { foo() {} }; descriptor(A.prototype, "foo").writable');
shouldBe('class A { foo() {} }; A.prototype.foo = 3; A.prototype.foo', '3');
shouldBeFalse('class A { foo() {} }; descriptor(A.prototype, "foo").enumerable');
shouldBeFalse('class A { foo() {} }; enumeratedProperties(A.prototype).includes("foo")');
shouldBeFalse('class A { foo() {} }; enumeratedProperties(new A).includes("foo")');
shouldBeTrue('class A { foo() {} }; descriptor(A.prototype, "foo").configurable');
shouldBe('class A { foo() {} }; Object.defineProperty(A.prototype, "foo", {value: "bar"}); A.prototype.foo', '"bar"');

shouldBe('class A { get foo() {} }; descriptor(A.prototype, "foo").writable', 'undefined');
shouldBe('class A { get foo() { return 5; } }; A.prototype.foo = 3; A.prototype.foo', '5');
shouldBeFalse('class A { get foo() {} }; descriptor(A.prototype, "foo").enumerable');
shouldBeFalse('class A { get foo() {} }; enumeratedProperties(A.prototype).includes("foo")');
shouldBeFalse('class A { get foo() {} }; enumeratedProperties(new A).includes("foo")');
shouldBeTrue('class A { get foo() {} }; descriptor(A.prototype, "foo").configurable');
shouldBe('class A { get foo() {} }; Object.defineProperty(A.prototype, "foo", {value: "bar"}); A.prototype.foo', '"bar"');

shouldBeTrue('class A { }; descriptor(A.prototype, "constructor").writable');
shouldBe('class A { }; A.prototype.constructor = 3; A.prototype.constructor', '3');
shouldBeTrue('class A { }; x = {}; A.prototype.constructor = function () { return x; }; (new A) instanceof A');
shouldBeFalse('class A { }; descriptor(A.prototype, "constructor").enumerable');
shouldBeFalse('class A { }; enumeratedProperties(A.prototype).includes("constructor")');
shouldBeFalse('class A { }; enumeratedProperties(new A).includes("constructor")');
shouldBeTrue('class A { }; descriptor(A.prototype, "constructor").configurable');
shouldBe('class A { }; Object.defineProperty(A.prototype, "constructor", {value: "bar"}); A.prototype.constructor', '"bar"');

var successfullyParsed = true;
