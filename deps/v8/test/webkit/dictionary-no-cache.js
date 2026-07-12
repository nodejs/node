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

description("Test to ensure that we handle caching of prototype chains containing dictionaries.");

var Test = function(){};

var methodCount = 65;

for (var i = 0; i < methodCount; i++){
    Test.prototype['myMethod' + i] = function(){};
}

var test1 = new Test();

for (var k in test1);

Test.prototype.myAdditionalMethod = function(){};
var test2 = new Test();
var j = k;
var foundNewPrototypeProperty = false;
for (var k in test2){
    if ("myAdditionalMethod" == k) foundNewPrototypeProperty = true;
}
shouldBeTrue('foundNewPrototypeProperty');

var Test = function(){};
for (var i = 0; i < methodCount; i++){
    Test.prototype['myMethod' + i] = function(){};
}
var test1 = new Test();

for (var k in test1);

delete (Test.prototype)[k]
var test2 = new Test();
var j = k;
var foundRemovedPrototypeProperty = false;
for (var k in test2){
    if (j == k) foundRemovedPrototypeProperty = true;
}
shouldBeFalse("foundRemovedPrototypeProperty");

var Test = function(){};
for (var i = 0; i < methodCount; i++){
    Test.prototype['myMethod' + i] = function(){};
}

function update(test) {
    test.newProperty = true;
}
var test1 = new Test();
update(test1);

var test2 = new Test();
update(test2);

var test3 = new Test();
update(test3);
var calledNewPrototypeSetter = false;
Test.prototype.__defineSetter__("newProperty", function(){ calledNewPrototypeSetter = true; });
var test4 = new Test();
update(test4);
shouldBeTrue('calledNewPrototypeSetter');

var test4 = {__proto__:{prop:"on prototype"}};
for (var i = 0; i < 200; i++)
    test4[i]=[i];

var test5 = {__proto__:{__proto__:{prop:"on prototype's prototype"}}};
for (var i = 0; i < 200; i++)
    test5[i]=[i];

getTestProperty = function(o) {
    return o.prop;
}

getTestProperty(test4);
getTestProperty(test4);
shouldBe("getTestProperty(test4)", '"on prototype"');
test4.prop = "on self";
shouldBe("getTestProperty(test4)", '"on self"');

getTestProperty = function(o) {
    return o.prop;
}

getTestProperty(test5);
getTestProperty(test5);
shouldBe("getTestProperty(test5)", '"on prototype\'s prototype"');
test5.prop = "on self";
shouldBe("getTestProperty(test5)", '"on self"');
