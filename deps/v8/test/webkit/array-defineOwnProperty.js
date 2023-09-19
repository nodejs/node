// Copyright 2013 the V8 project authors. All rights reserved.
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

description("This test checks the behavior of [[DefineOwnProperty]] applied to Array objects (see ES5.1 15.4.5.1).");

// Test that properties other than length, array indices are handled as normal.
shouldBeTrue("Object.defineProperty([], 'x', { get:function(){return true;} }).x");

// The length property can be set, and can be made read-only.
shouldBe("Object.defineProperty([], 'length', { value: 1 }).length", '1');
shouldBe("var a = Object.defineProperty([], 'length', { writable: false }); a[1] = 1; a.length", '0');
shouldBe("var a = Object.defineProperty([], 'length', { writable: false }); a.length = 1; a.length", '0');
// If writable is not specified, it should not change.
shouldBe("var a = Object.defineProperty([], 'length', {}); a.length = 1; a.length", '1');

// The length property can be replaced with an accessor, or made either enumerable or configurable.
shouldThrow("Object.defineProperty([], 'length', { get:function(){return true;} })");
shouldThrow("Object.defineProperty([], 'length', { enumerable: true })");
shouldThrow("Object.defineProperty([], 'length', { configurable: true })");
shouldThrow("Object.defineProperty(Object.defineProperty([], 'length', { writable: false }), 'length', { writable: true })");

// The value of an indexed property can be set.
shouldBe("var a = Object.defineProperty([], '0', { value: 42 }); a[0]", '42');
// An indexed property can be made non-writable/enumerable/configurable.
shouldBe("var a = Object.defineProperty([42], '0', { writable: false }); a[0] = 1; a[0]", '42');
shouldBe("var a = Object.defineProperty([42], '0', { enumerable: false }); a[0] + Object.keys(a).length", '42');
shouldBe("var a = Object.defineProperty([42], '0', { configurable: false }); a.length = 0; a[0]", '42');
// An indexed property can be defined as an accessor.
shouldBe("var foo = 0; Object.defineProperty([], '0', { set:function(x){foo = x;} })[0] = 42; foo", '42');
shouldBeTrue("Object.defineProperty([], '0', { get:function(){return true;} })[0]")
// A configurable, non-writable property can be made writable, but a non-configurable one cannot.
shouldBeTrue("Object.defineProperty(Object.defineProperty([true], '0', { configurable:true, writable: false }), '0', { writable: true })[0]");
shouldThrow("Object.defineProperty(Object.defineProperty([true], '0', { configurable:false, writable: false }), '0', { writable: true })[0]");

// Reassigning the value is okay if the property is writable.
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: 1, writable:true }), '0', { value: 2 })[0]", '2');
// Reassigning the value is okay if the value doesn't change.
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: 1 }), '0', { value: 1 })[0]", '1');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: Number.NaN }), '0', { value: -Number.NaN })[0]", 'Number.NaN');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: 'okay'.substring(0,2) }), '0', { value: 'not ok'.substring(4,6) })[0]", '"ok"');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: true }), '0', { value: true })[0]", 'true');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: false }), '0', { value: false })[0]", 'false');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: null }), '0', { value: null })[0]", 'null');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: undefined }), '0', { value: undefined })[0]", 'undefined');
shouldBe("Object.defineProperty(Object.defineProperty([], '0', { value: Math }), '0', { value: Math })[0]", 'Math');
// Reassigning the value is not okay if the value changes.
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: 1 }), '0', { value: 2 })[0]");
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: 'okay' }), '0', { value: 'not ok' })[0]");
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: true }), '0', { value: false })[0]");
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: false }), '0', { value: true })[0]");
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: Math }), '0', { value: Object })[0]");
// Reassigning the value is not okay if the type changes.
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: null }), '0', { value: undefined })[0]");
shouldThrow("Object.defineProperty(Object.defineProperty([], '0', { value: undefined }), '0', { value: null })[0]");

Object.defineProperty(Array.prototype, "0", { set: function () { throw false; } });
Object.defineProperty(Array.prototype, "1", { set: function () { throw false; } });
var arrObj = [ , false ];
Object.defineProperty(arrObj, "0", { set: function (x) { this.set = x === 42; } });
shouldBeTrue("arrObj[0] = 42; arrObj.set;");
shouldBeTrue("arrObj[1] = true; arrObj[1];");

successfullyParsed = true;
