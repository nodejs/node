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

description('Tests for ES6 class syntax "super"');

var baseMethodValue = {};
var valueInSetter = null;

class Base {
    constructor() { }
    baseMethod() { return baseMethodValue; }
    chainMethod() { return 'base'; }
    static staticMethod() { return 'base3'; }
}

class Derived extends Base {
    constructor() { super(); }
    chainMethod() { return [super.chainMethod(), 'derived']; }
    callBaseMethod() { return super.baseMethod(); }
    get callBaseMethodInGetter() { return super['baseMethod'](); }
    set callBaseMethodInSetter(x) { valueInSetter = super.baseMethod(); }
    get baseMethodInGetterSetter() { return super.baseMethod; }
    set baseMethodInGetterSetter(x) { valueInSetter = super['baseMethod']; }
    static staticMethod() { return super.staticMethod(); }
}

class SecondDerived extends Derived {
    constructor() { super(); }
    chainMethod() { return super.chainMethod().concat(['secondDerived']); }
}

shouldBeTrue('(new Base) instanceof Base');
shouldBeTrue('(new Derived) instanceof Derived');
shouldBe('(new Derived).callBaseMethod()', 'baseMethodValue');
shouldBe('x = (new Derived).callBaseMethod; x()', 'baseMethodValue');
shouldBe('(new Derived).callBaseMethodInGetter', 'baseMethodValue');
shouldBe('(new Derived).callBaseMethodInSetter = 1; valueInSetter', 'baseMethodValue');
shouldBe('(new Derived).baseMethodInGetterSetter', '(new Base).baseMethod');
shouldBe('(new Derived).baseMethodInGetterSetter = 1; valueInSetter', '(new Base).baseMethod');
shouldBe('Derived.staticMethod()', '"base3"');
shouldBe('(new SecondDerived).chainMethod()', '["base", "derived", "secondDerived"]');
shouldNotThrow('x = class extends Base { constructor() { super(); } super() {} }');
shouldThrow('x = class extends Base { constructor() { super(); } method() { super() } }',
    '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('x = class extends Base { constructor() { super(); } method() { super } }', '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('x = class extends Base { constructor() { super(); } method() { return new super } }', '"SyntaxError: \'super\' keyword unexpected here"');
// shouldBeTrue('(new x).method() instanceof Base');
// shouldBeFalse('(new x).method() instanceof x');
shouldNotThrow('x = class extends Base { constructor() { super(); } method1() { delete (super.foo) } method2() { delete super["foo"] } }');
shouldThrow('(new x).method1()', '"ReferenceError: Unsupported reference to \'super\'"');
shouldThrow('(new x).method2()', '"ReferenceError: Unsupported reference to \'super\'"');
shouldBeTrue('new (class { constructor() { return undefined; } }) instanceof Object');
shouldBeTrue('new (class { constructor() { return 1; } }) instanceof Object');
shouldThrow('new (class extends Base { constructor() { return undefined } })');
shouldBeTrue('new (class extends Base { constructor() { super(); return undefined } }) instanceof Object');
shouldBe('x = { }; new (class extends Base { constructor() { return x } });', 'x');
shouldBeFalse('x instanceof Base');
shouldThrow('new (class extends Base { constructor() { } })', '"ReferenceError: this is not defined"');
shouldThrow('new (class extends Base { constructor() { return 1; } })', '"TypeError: Derived constructors may only return object or undefined"');
shouldThrow('new (class extends null { constructor() { return undefined } })');
shouldThrow('new (class extends null { constructor() { super(); return undefined } })', '"TypeError: Super constructor null of anonymous class is not a constructor"');
shouldBe('x = { }; new (class extends null { constructor() { return x } });', 'x');
shouldBeTrue('x instanceof Object');
shouldThrow('new (class extends null { constructor() { } })', '"ReferenceError: this is not defined"');
shouldThrow('new (class extends null { constructor() { return 1; } })', '"TypeError: Derived constructors may only return object or undefined"');
shouldThrow('new (class extends null { constructor() { super() } })', '"TypeError: Super constructor null of anonymous class is not a constructor"');
shouldThrow('new (class { constructor() { super() } })', '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('function x() { super(); }', '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('new (class extends Object { constructor() { function x() { super() } } })', '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('new (class extends Object { constructor() { function x() { super.method } } })', '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('function x() { super.method(); }', '"SyntaxError: \'super\' keyword unexpected here"');
shouldThrow('function x() { super(); }', '"SyntaxError: \'super\' keyword unexpected here"');

var successfullyParsed = true;
