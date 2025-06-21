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

description(
"This performs a number of different tests on JavaScript getters and setters."
);

debug("the get set object declaration syntax");
var o1 = { 'a':7, get b() { return this.a + 1 }, set b(x) { this.a = x } }
shouldBe("o1.b", "8");
o1.b = 10;
shouldBe("o1.b", "11");

debug("__defineGetter__ and __defineSetter__");
var o2 = new Object()
o2.a = 7;
o2.__defineGetter__('b', function getB() { return this.a + 1} )
o2.__defineSetter__('b', function setB(x) { this.a = x})
shouldBe("o2.b", "8");
o2.b = 10;
shouldBe("o2.b", "11");

debug("Setting a value without having a setter");
var o3 = { get x() { return 42; } }
shouldBe("o3.x = 10; o3.x", "42");

debug("Getting a value without having a getter");
var o4 = { set x(y) { }}
shouldBeUndefined("o4.x");

debug("__lookupGetter__ and __lookupSetter__");
var o4 = new Object()
function getB() { return this.a }
function setB(x) { this.a = x }
o4.__defineGetter__('b', getB)
o4.__defineSetter__('b', setB)

shouldBe("o4.__lookupGetter__('b')", "getB");
shouldBe("o4.__lookupSetter__('b')", "setB");

debug("__defineGetter__ and __defineSetter__ with various invalid arguments");
var numExceptions = 0;
var o5 = new Object();
shouldThrow("o5.__defineSetter__('a', null)");
shouldThrow("o5.__defineSetter__('a', o5)");
shouldThrow("o5.__defineGetter__('a', null)");
shouldThrow("o5.__defineGetter__('a', o5)");

debug("setters and getters with exceptions");
var o6 = { get x() { throw 'Exception in get'}, set x(f) { throw 'Exception in set'}}
var x = 0;
var numExceptions = 0;
shouldThrow("x = o6.x");
shouldBe("x", "0");
shouldThrow("o6.x = 42");

debug("Defining a setter should also define a getter for the same property which returns undefined. Thus, a getter defined on the prototype should not be called.");
o7 = { 'a':7, set x(b) { this.a = b; }}
o7.prototype = { get x() { return 42; }}
shouldBeUndefined("o7.x")

debug("If an object has a property and its prototype has a setter function for that property, then setting the property should set the property directly and not call the setter function.");
var o8 = new Object()
o8.numSets = 0;
o8.x = 10;
o8.__proto__.__defineSetter__('x', function() { this.numSets++; })
o8.x = 20;
shouldBe("o8.numSets", "0");

({getter:"foo", b:"bar"});
testObj=({get getter(){return 'getter was called.'}, b: 'bar'})
shouldBe("typeof testObj.getter", "'string'");

debug("the get set with string property name");
var o9 = { 'a':7, get 'b'() { return this.a + 1 }, set 'b'(x) { this.a = x } }
shouldBe("o9.b", "8");
o9.b = 10;
shouldBe("o9.b", "11");

debug("the get set with numeric property name");
var o10 = { 'a':7, get 42() { return this.a + 1 }, set 42(x) { this.a = x } }
shouldBe("o10[42]", "8");
o10[42] = 10;
shouldBe("o10[42]", "11");
