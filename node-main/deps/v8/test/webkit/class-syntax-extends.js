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

description('Tests for ES6 class syntax "extends"');

class Base {
    constructor() { }
    baseMethod() { return 'base'; }
    overridenMethod() { return 'base'; }
    static staticBaseMethod() { return 'base'; }
    static staticOverridenMethod() { return 'base'; }
}

class Derived extends Base {
    constructor() { super(); }
    overridenMethod() { return 'derived'; }
    static staticOverridenMethod() { return 'derived'; }
}

shouldBeTrue('(new Base) instanceof Base');
shouldBe('Object.getPrototypeOf(new Base)', 'Base.prototype');
shouldBeTrue('(new Derived) instanceof Derived');
shouldBe('Object.getPrototypeOf(new Derived)', 'Derived.prototype');
shouldBe('Object.getPrototypeOf(Derived.prototype)', 'Base.prototype');
shouldBe('(new Derived).baseMethod()', '"base"');
shouldBe('(new Derived).overridenMethod()', '"derived"');
shouldBe('Derived.staticBaseMethod()', '"base"');
shouldBe('Derived.staticOverridenMethod()', '"derived"');

shouldThrow('x = class extends', '"SyntaxError: Unexpected end of input"');
shouldThrow('x = class extends', '"SyntaxError: Unexpected end of input"');
shouldThrow('x = class extends Base {', '"SyntaxError: Unexpected end of input"');
shouldNotThrow('x = class extends Base { }');
shouldNotThrow('x = class extends Base { constructor() { } }');
shouldBe('x.__proto__', 'Base');
shouldBe('Object.getPrototypeOf(x)', 'Base');
shouldBe('x.prototype.__proto__', 'Base.prototype');
shouldBe('Object.getPrototypeOf(x.prototype)', 'Base.prototype');
shouldBe('x = class extends null { constructor() { } }; x.__proto__', 'Function.prototype');
shouldBe('x.__proto__', 'Function.prototype');
shouldThrow('x = class extends 3 { constructor() { } }; x.__proto__', '"TypeError: Class extends value 3 is not a constructor or null"');
shouldThrow('x = class extends "abc" { constructor() { } }; x.__proto__', '"TypeError: Class extends value abc is not a constructor or null"');
shouldNotThrow('baseWithBadPrototype = function () {}; baseWithBadPrototype.prototype = 3; new baseWithBadPrototype');
shouldThrow('x = class extends baseWithBadPrototype { constructor() { } }', '"TypeError: Class extends value does not have valid prototype property 3"');
shouldNotThrow('baseWithBadPrototype.prototype = "abc"');
shouldThrow('x = class extends baseWithBadPrototype { constructor() { } }', '"TypeError: Class extends value does not have valid prototype property abc"');
shouldNotThrow('baseWithBadPrototype.prototype = null; x = class extends baseWithBadPrototype { constructor() { } }');

shouldThrow('x = 1; c = class extends ++x { constructor() { } };');
shouldThrow('x = 1; c = class extends x++ { constructor() { } };');
shouldThrow('x = 1; c = class extends (++x) { constructor() { } };');
shouldThrow('x = 1; c = class extends (x++) { constructor() { } };');
shouldBe('x = 1; try { c = class extends (++x) { constructor() { } } } catch (e) { }; x', '2');
shouldBe('x = 1; try { c = class extends (x++) { constructor() { } } } catch (e) { }; x', '2');

shouldNotThrow('namespace = {}; namespace.A = class { }; namespace.B = class extends namespace.A { }');
shouldNotThrow('namespace = {}; namespace.A = class A { }; namespace.B = class B extends namespace.A { }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends namespace.A { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class A { constructor() { } }; namespace.B = class B extends namespace.A { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (namespace.A) { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends namespace["A"] { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; function getClassA() { return namespace.A }; namespace.B = class extends getClassA() { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; function getClass(prop) { return namespace[prop] }; namespace.B = class extends getClass("A") { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (false||null||namespace.A) { constructor() { } }');
shouldThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends false||null||namespace.A { constructor() { } }');
shouldNotThrow('x = 1; namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (x++, namespace.A) { constructor() { } };');
shouldThrow('x = 1; namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (namespace.A, x++) { constructor() { } };');
shouldThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends new namespace.A { constructor() { } }');
shouldThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends new namespace.A() { constructor() { } }');
shouldBe('x = 1; namespace = {}; namespace.A = class { constructor() { } }; try { namespace.B = class extends (x++, namespace.A) { constructor() { } } } catch (e) { } x', '2');
shouldBe('x = 1; namespace = {}; namespace.A = class { constructor() { } }; try { namespace.B = class extends (namespace.A, x++) { constructor() { } } } catch (e) { } x', '2');

shouldBe('Object.getPrototypeOf((class { constructor () { } }).prototype)', 'Object.prototype');
shouldBe('Object.getPrototypeOf((class extends null { constructor () { super(); } }).prototype)', 'null');
shouldThrow('new (class extends undefined { constructor () { this } })', '"TypeError: Class extends value undefined is not a constructor or null"');
shouldThrow('new (class extends undefined { constructor () { super(); } })', '"TypeError: Class extends value undefined is not a constructor or null"');
shouldThrow('x = {}; new (class extends undefined { constructor () { return x; } })', '"TypeError: Class extends value undefined is not a constructor or null"');
shouldThrow('y = 12; new (class extends undefined { constructor () { return y; } })', '"TypeError: Class extends value undefined is not a constructor or null"');
shouldBeTrue ('class x {}; new (class extends null { constructor () { return new x; } }) instanceof x');
shouldThrow('new (class extends null { constructor () { this; } })', '"ReferenceError: Must call super constructor in derived class before accessing \'this\' or returning from derived constructor"');
shouldThrow('new (class extends null { constructor () { super(); } })', '"TypeError: Super constructor null of anonymous class is not a constructor"');
shouldBe('x = {}; new (class extends null { constructor () { return x } })', 'x');
shouldThrow('y = 12; new (class extends null { constructor () { return y; } })', '"TypeError: Derived constructors may only return object or undefined"');
shouldBeTrue ('class x {}; new (class extends null { constructor () { return new x; } }) instanceof x');
shouldBe('x = null; Object.getPrototypeOf((class extends x { }).prototype)', 'null');
shouldBeTrue('Object.prototype.isPrototypeOf(class { })');
shouldBeTrue('Function.prototype.isPrototypeOf(class { })');

var successfullyParsed = true;
