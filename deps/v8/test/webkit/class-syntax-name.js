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

description('Tests for ES6 class name semantics in class statements and expressions');

function runTestShouldBe(statement, result) {
    shouldBe(statement, result);
    shouldBe("'use strict'; " + statement, result);
}

function runTestShouldBeTrue(statement) {
    shouldBeTrue(statement);
    shouldBeTrue("'use strict'; " + statement);
}

function runTestShouldThrow(statement) {
    shouldThrow(statement);
    shouldThrow("'use strict'; " + statement);
}

function runTestShouldNotThrow(statement) {
    shouldNotThrow(statement);
    shouldNotThrow("'use strict'; " + statement);
}

// Class statement. Class name added to global scope. Class name is available inside class scope and in global scope.
debug('Class statement');
runTestShouldThrow("A");
runTestShouldThrow("class {}");
runTestShouldThrow("class { constructor() {} }");
runTestShouldNotThrow("class A { constructor() {} }");
runTestShouldBe("class A { constructor() {} }; A.toString()", "'class A { constructor() {} }'");
runTestShouldBeTrue("class A { constructor() {} }; (new A) instanceof A");
runTestShouldBe("class A { constructor() { this.base = A; } }; (new A).base.toString()", "'class A { constructor() { this.base = A; } }'");
runTestShouldNotThrow("class A { constructor() {} }; class B extends A {};");
runTestShouldBe("class A { constructor() {} }; class B extends A { constructor() {} }; B.toString()", "'class B extends A { constructor() {} }'");
runTestShouldBeTrue("class A { constructor() {} }; class B extends A {}; (new B) instanceof A");
runTestShouldBeTrue("class A { constructor() {} }; class B extends A {}; (new B) instanceof B");
runTestShouldBe("class A { constructor() {} }; class B extends A { constructor() { super(); this.base = A; this.derived = B; } }; (new B).base.toString()", "'class A { constructor() {} }'");
runTestShouldBe("class A { constructor() {} }; class B extends A { constructor() { super(); this.base = A; this.derived = B; } }; (new B).derived.toString()", "'class B extends A { constructor() { super(); this.base = A; this.derived = B; } }'");

// Class expression. Class name not added to scope. Class name is available inside class scope.
debug(''); debug('Class expression');
runTestShouldThrow("A");
runTestShouldNotThrow("(class {})");
runTestShouldNotThrow("(class { constructor(){} })");
runTestShouldBe("typeof (class {})", '"function"');
runTestShouldNotThrow("(class A {})");
runTestShouldBe("typeof (class A {})", '"function"');
runTestShouldThrow("(class A {}); A");
runTestShouldNotThrow("new (class A {})");
runTestShouldBe("typeof (new (class A {}))", '"object"');
runTestShouldNotThrow("(new (class A { constructor() { this.base = A; } })).base");
runTestShouldBe("(new (class A { constructor() { this.base = A; } })).base.toString()", '"class A { constructor() { this.base = A; } }"');
runTestShouldNotThrow("class A {}; (class B extends A {})");
runTestShouldThrow("class A {}; (class B extends A {}); B");
runTestShouldNotThrow("class A {}; new (class B extends A {})");
runTestShouldNotThrow("class A {}; new (class B extends A { constructor() { super(); this.base = A; this.derived = B; } })");
runTestShouldBeTrue("class A {}; (new (class B extends A { constructor() { super(); this.base = A; this.derived = B; } })) instanceof A");
runTestShouldBe("class A { constructor() {} }; (new (class B extends A { constructor() { super(); this.base = A; this.derived = B; } })).base.toString()", "'class A { constructor() {} }'");
runTestShouldBe("class A { constructor() {} }; (new (class B extends A { constructor() { super(); this.base = A; this.derived = B; } })).derived.toString()", "'class B extends A { constructor() { super(); this.base = A; this.derived = B; } }'");

// Assignment of a class expression to a variable. Variable name available in scope, class name is not. Class name is available inside class scope.
debug(''); debug('Class expression assignment to variable');
runTestShouldThrow("A");
runTestShouldNotThrow("var VarA = class {}");
runTestShouldBe("var VarA = class { constructor() {} }; VarA.toString()", "'class { constructor() {} }'");
runTestShouldThrow("VarA");
runTestShouldNotThrow("var VarA = class A { constructor() {} }");
runTestShouldBe("var VarA = class A { constructor() {} }; VarA.toString()", "'class A { constructor() {} }'");
runTestShouldThrow("var VarA = class A { constructor() {} }; A.toString()");
runTestShouldBeTrue("var VarA = class A { constructor() {} }; (new VarA) instanceof VarA");
runTestShouldBe("var VarA = class A { constructor() { this.base = A; } }; (new VarA).base.toString()", "'class A { constructor() { this.base = A; } }'");
runTestShouldNotThrow("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { constructor() {} };");
runTestShouldThrow("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { constructor() {} }; B");
runTestShouldBe("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { constructor() {} }; VarB.toString()", "'class B extends VarA { constructor() {} }'");
runTestShouldBeTrue("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { }; (new VarB) instanceof VarA");
runTestShouldBeTrue("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { }; (new VarB) instanceof VarB");
runTestShouldBeTrue("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { constructor() { super(); this.base = VarA; this.derived = B; this.derivedVar = VarB; } }; (new VarB).base === VarA");
runTestShouldBeTrue("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { constructor() { super(); this.base = VarA; this.derived = B; this.derivedVar = VarB; } }; (new VarB).derived === VarB");
runTestShouldBeTrue("var VarA = class A { constructor() {} }; var VarB = class B extends VarA { constructor() { super(); this.base = VarA; this.derived = B; this.derivedVar = VarB; } }; (new VarB).derivedVar === VarB");

// FIXME: Class statement binding should be like `let`, not `var`.
debug(''); debug('Class statement binding in other circumstances');
runTestShouldThrow("var result = A; result");
runTestShouldThrow("var result = A; class A {}; result");
runTestShouldThrow("class A { constructor() { A = 1; } }; new A");
runTestShouldBe("class A { constructor() { } }; A = 1; A", "1");
runTestShouldNotThrow("class A {}; var result = A; result");
shouldBe("eval('var Foo = 10'); Foo", "10");
shouldThrow("'use strict'; eval('var Foo = 10'); Foo");
shouldBe("eval('class Bar { constructor() {} }; Bar.toString()')", "'class Bar { constructor() {} }'");
shouldThrow("'use strict'; eval('class Bar { constructor() {} }'); Bar.toString()");
