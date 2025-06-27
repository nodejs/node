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

description('Tests for ES6 class syntax expressions');

var constructorCallCount = 0;
const staticMethodValue = [1];
const instanceMethodValue = [2];
const getterValue = [3];
var setterValue = undefined;
var A = class {
    constructor() { constructorCallCount++; }
    static someStaticMethod() { return staticMethodValue; }
    static get someStaticGetter() { return getterValue; }
    static set someStaticSetter(value) { setterValue = value; }
    someInstanceMethod() { return instanceMethodValue; }
    get someGetter() { return getterValue; }
    set someSetter(value) { setterValue = value; }
};

shouldBe("constructorCallCount", "0");
shouldBe("A.someStaticMethod()", "staticMethodValue");
shouldBe("A.someStaticGetter", "getterValue");
shouldBe("setterValue = undefined; A.someStaticSetter = 123; setterValue", "123");
shouldBe("(new A).someInstanceMethod()", "instanceMethodValue");
shouldBe("constructorCallCount", "1");
shouldBe("(new A).someGetter", "getterValue");
shouldBe("constructorCallCount", "2");
shouldBe("(new A).someGetter", "getterValue");
shouldBe("setterValue = undefined; (new A).someSetter = 789; setterValue", "789");
shouldBe("(new A).__proto__", "A.prototype");
shouldBe("A.prototype.constructor", "A");

shouldThrow("x = class", "'SyntaxError: Unexpected end of input'");
shouldThrow("x = class {", "'SyntaxError: Unexpected end of input'");
shouldThrow("x = class { ( }", '"SyntaxError: Unexpected token \'(\'"');
shouldNotThrow("x = class {}");

shouldThrow("x = class { constructor() {} constructor() {} }", "'SyntaxError: A class may only have one constructor'");
shouldThrow("x = class { get constructor() {} }", "'SyntaxError: Class constructor may not be an accessor'");
shouldThrow("x = class { set constructor() {} }", "'SyntaxError: Class constructor may not be an accessor'");
shouldNotThrow("x = class { constructor() {} static constructor() { return staticMethodValue; } }");
shouldBe("x = class { constructor() {} static constructor() { return staticMethodValue; } }; x.constructor()", "staticMethodValue");

shouldThrow("x = class { constructor() {} static prototype() {} }", '"SyntaxError: Classes may not have a static property named \'prototype\'"');
shouldThrow("x = class { constructor() {} static get prototype() {} }", '"SyntaxError: Classes may not have a static property named \'prototype\'"');
shouldThrow("x = class { constructor() {} static set prototype() {} }", '"SyntaxError: Classes may not have a static property named \'prototype\'"');
shouldNotThrow("x = class  { constructor() {} prototype() { return instanceMethodValue; } }");
shouldBe("x = class { constructor() {} prototype() { return instanceMethodValue; } }; (new x).prototype()", "instanceMethodValue");

shouldNotThrow("x = class { constructor() {} set foo(a) {} }");
shouldNotThrow("x = class { constructor() {} set foo({x, y}) {} }");
shouldThrow("x = class { constructor() {} set foo() {} }");
shouldThrow("x = class { constructor() {} set foo(a, b) {} }");
shouldNotThrow("x = class { constructor() {} get foo() {} }");
shouldThrow("x = class { constructor() {} get foo(x) {} }");
shouldThrow("x = class { constructor() {} get foo({x, y}) {} }");

var successfullyParsed = true;
