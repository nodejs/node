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

description("Test to ensure correct behaviour of Object.defineProperty");

shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {}), 'foo'))",
         "JSON.stringify({writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {get:undefined}), 'foo'))",
         "JSON.stringify({enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, writable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, writable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: true, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, enumerable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, enumerable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: true, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, configurable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, configurable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: true})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty([1,2,3], 'foo', {value:1, configurable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: true})");
shouldBe("Object.getOwnPropertyDescriptor(Object.defineProperty([1,2,3], '1', {value:'foo', configurable: true}), '1').value", "'foo'");
shouldBe("a=[1,2,3], Object.defineProperty(a, '1', {value:'foo', configurable: true}), a[1]", "'foo'");
shouldBe("Object.getOwnPropertyDescriptor(Object.defineProperty([1,2,3], '1', {get:getter, configurable: true}), '1').get", "getter");
shouldThrow("Object.defineProperty([1,2,3], '1', {get:getter, configurable: true})[1]", "'called getter'");

shouldThrow("Object.defineProperty()");
shouldThrow("Object.defineProperty(null)");
shouldThrow("Object.defineProperty('foo')");
shouldThrow("Object.defineProperty({})");
shouldThrow("Object.defineProperty({}, 'foo')");
shouldThrow("Object.defineProperty({}, 'foo', {get:undefined, value:true}).foo");
shouldBeTrue("Object.defineProperty({get foo() { return true; } }, 'foo', {configurable:false}).foo");

function createUnconfigurableProperty(o, prop, writable, enumerable) {
    writable = writable || false;
    enumerable = enumerable || false;
    return Object.defineProperty(o, prop, {value:1, configurable:false, writable: writable, enumerable: enumerable});
}
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo'), 'foo', {configurable: true})");
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo'), 'foo', {writable: true})");
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo'), 'foo', {enumerable: true})");
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo', false, true), 'foo', {enumerable: false}), 'foo'");

shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty(createUnconfigurableProperty({}, 'foo', true), 'foo', {writable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");

shouldThrow("Object.defineProperty({}, 'foo', {value:1, get: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {value:1, set: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {writable:true, get: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {writable:true, set: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {get: null})");
shouldThrow("Object.defineProperty({}, 'foo', {set: null})");
function getter(){ throw "called getter"; }
function getter1(){ throw "called getter1"; }
function setter(){ throw "called setter"; }
function setter1(){ throw "called setter1"; }
shouldThrow("Object.defineProperty({}, 'foo', {set: setter}).foo='test'", "'called setter'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter}), 'foo', {get: getter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter}), 'foo', {value: 1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {get: getter1}).foo", "'called getter1'");
shouldBeTrue("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {value: true}).foo");
shouldBeTrue("'foo' in Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {writable: true})");
shouldBeUndefined("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {writable: true}).foo");
shouldThrow("Object.defineProperty({}, 'foo', {get: getter}).foo", "'called getter'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter}), 'foo', {get: getter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {get: getter1}).foo", "'called getter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");
shouldBeTrue("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {value: true}).foo");
shouldBeUndefined("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {writable: true}).foo");
shouldBeTrue("'foo' in Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {writable: true})");

shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {get: getter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true}), 'foo', {get: getter1}).foo", "'called getter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");

// Should be able to redefine an non-writable property, if it is configurable.
shouldBe("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true, writable: false}), 'foo', {value: 2, configurable: true, writable: true}).foo", "2");
shouldBe("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true, writable: false}), 'foo', {value: 2, configurable: true, writable: false}).foo", "2");

// Should be able to redefine an non-writable, non-configurable property, with the same value and attributes.
shouldBe("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: false, writable: false}), 'foo', {value: 1, configurable: false, writable: false}).foo", "1");

// Should be able to redefine a configurable accessor, with the same or with different attributes.
// Check the accessor function changed.
shouldBe("Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true, enumerable: true}), 'foo', {get: function() { return 2; }, configurable: true, enumerable: false}).foo", "2");
shouldBe("Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true, enumerable: false}), 'foo', {get: function() { return 2; }, configurable: true, enumerable: false}).foo", "2");
// Check the attributes changed.
shouldBeFalse("var result = false; var o = Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true, enumerable: true}), 'foo', {get: function() { return 2; }, configurable: true, enumerable: false}); for (x in o) result = true; result");
shouldBeTrue("var result = false; var o = Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true, enumerable: false}), 'foo', {get: function() { return 2; }, configurable: true, enumerable: true}); for (x in o) result = true; result");

// Should be able to define an non-configurable accessor.
shouldBeUndefined("var o = Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true}); delete o.foo; o.foo");
shouldBe("var o = Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: false}); delete o.foo; o.foo", '1');
shouldBe("Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true}), 'foo', {value:2}).foo", "2");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: false}), 'foo', {value:2}).foo");

// Defining a data descriptor over an accessor should result in a read only property.
shouldBe("var o = Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return 1; }, configurable: true}), 'foo', {value:2}); o.foo = 3; o.foo", "2");

// Trying to redefine a non-configurable property as configurable should throw.
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {value:2, configurable: true})");
shouldThrow("Object.defineProperty(Object.defineProperty([], 'foo', {value: 1}), 'foo', {value:2, configurable: true})");

// Test an object with a getter setter.
// Either accessor may be omitted or replaced with undefined, or both may be replaced with undefined.
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}); o.foo", '42')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:function(x){this.result = x;}}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("var o = Object.defineProperty({}, 'foo', {set:function(x){this.result = x;}}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty({}, 'foo', {set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:undefined}); o.foo", '42')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:undefined}); o.foo = 42; o.result;", 'undefined');
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}}); o.foo", '42')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}}); o.foo = 42; o.result;", 'undefined');
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:undefined}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:undefined}); o.foo = 42; o.result;", 'undefined');
// Test replacing or removing either the getter or setter individually.
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:function(){return 13;}}); o.foo", '13')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:function(){return 13;}}); o.foo = 42; o.result;", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:undefined}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:undefined}); o.foo = 42; o.result;", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:function(){this.result = 13;}}); o.foo", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:function(){this.result = 13;}}); o.foo = 42; o.result;", '13')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:undefined}); o.foo", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:undefined}); o.foo = 42; o.result;", 'undefined')

// Repeat of the above, in strict mode.
// Either accessor may be omitted or replaced with undefined, or both may be replaced with undefined.
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}); o.foo", '42')
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:undefined, set:function(x){this.result = x;}}); o.foo", 'undefined')
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:undefined, set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {set:function(x){this.result = x;}}); o.foo", 'undefined')
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:undefined}); o.foo", '42')
shouldThrow("'use strict'; var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:undefined}); o.foo = 42; o.result;");
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}}); o.foo", '42')
shouldThrow("'use strict'; var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}}); o.foo = 42; o.result;");
shouldBe("'use strict'; var o = Object.defineProperty({}, 'foo', {get:undefined, set:undefined}); o.foo", 'undefined')
shouldThrow("'use strict'; var o = Object.defineProperty({}, 'foo', {get:undefined, set:undefined}); o.foo = 42; o.result;");
// Test replacing or removing either the getter or setter individually.
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:function(){return 13;}}); o.foo", '13')
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:function(){return 13;}}); o.foo = 42; o.result;", '42')
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:undefined}); o.foo", 'undefined')
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:undefined}); o.foo = 42; o.result;", '42')
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:function(){this.result = 13;}}); o.foo", '42')
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:function(){this.result = 13;}}); o.foo = 42; o.result;", '13')
shouldBe("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:undefined}); o.foo", '42')
shouldThrow("'use strict'; var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:undefined}); o.foo = 42; o.result;")

Object.defineProperty(Object.prototype, 0, {get:function(){ return false; }, configurable:true})
shouldBeTrue("0 in Object.prototype");
shouldBeTrue("'0' in Object.prototype");
delete Object.prototype[0];

Object.defineProperty(Object.prototype, 'readOnly', {value:true, configurable:true, writable:false})
shouldBeTrue("var o = {}; o.readOnly = false; o.readOnly");
shouldThrow("'use strict'; var o = {}; o.readOnly = false; o.readOnly");
delete Object.prototype.readOnly;

// Check the writable attribute is set correctly when redefining an accessor as a data descriptor.
shouldBeFalse("Object.getOwnPropertyDescriptor(Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return false; }, configurable: true}), 'foo', {value:false}), 'foo').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return false; }, configurable: true}), 'foo', {value:false, writable: false}), 'foo').writable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Object.defineProperty(Object.defineProperty({}, 'foo', {get: function() { return false; }, configurable: true}), 'foo', {value:false, writable: true}), 'foo').writable");

// If array length is read-only, [[Put]] should fail.
shouldBeFalse("var a = Object.defineProperty([], 'length', {writable: false}); a[0] = 42; 0 in a;");
shouldThrow("'use strict'; var a = Object.defineProperty([], 'length', {writable: false}); a[0] = 42; 0 in a;");

// If array property is read-only, [[Put]] should fail.
shouldBe("var a = Object.defineProperty([42], '0', {writable: false}); a[0] = false; a[0];", '42');
shouldThrow("'use strict'; var a = Object.defineProperty([42], '0', {writable: false}); a[0] = false; a[0];");

// If array property is an undefined setter, [[Put]] should fail.
shouldBeUndefined("var a = Object.defineProperty([], '0', {set: undefined}); a[0] = 42; a[0];");
shouldThrow("'use strict'; var a = Object.defineProperty([], '0', {set: undefined}); a[0] = 42; a[0];");

function testObject()
{
    // Test case from https://bugs.webkit.org/show_bug.cgi?id=38636
    Object.defineProperty(anObj, 'slot1', {value: 'foo', enumerable: true});
    Object.defineProperty(anObj, 'slot2', {value: 'bar', writable: true});
    Object.defineProperty(anObj, 'slot3', {value: 'baz', enumerable: false});
    Object.defineProperty(anObj, 'slot4', {value: 'goo', configurable: false});
    shouldBe("anObj.slot1", '"foo"');
    shouldBe("anObj.slot2", '"bar"');
    anObj.slot2 = 'bad value';
    shouldBeTrue("anObj.propertyIsEnumerable('slot1')");
    shouldBeFalse("anObj.propertyIsEnumerable('slot2')");
    delete anObj.slot4;
    shouldBe("anObj.slot4", '"goo"');

    // Test case from https://bugs.webkit.org/show_bug.cgi?id=48911
    Object.defineProperty(Object.getPrototypeOf(anObj), 'slot5', {get: function() { return this._Slot5; }, set: function(v) { this._Slot5 = v; }, configurable: false});
    anObj.slot5 = 123;
    shouldBe("anObj.slot5", '123');
    shouldBe("anObj._Slot5", '123');
    shouldBeUndefined("Object.getOwnPropertyDescriptor(anObj, 'slot5')");
    Object.defineProperty(anObj, 'slot5', { value: 456 });
    shouldBe("anObj.slot5", '456');
    shouldBe("anObj._Slot5", '123');
    shouldBe("Object.getOwnPropertyDescriptor(anObj, 'slot5').value", '456');
}
var anObj = {};
testObject();
var anObj = this;
testObject();
