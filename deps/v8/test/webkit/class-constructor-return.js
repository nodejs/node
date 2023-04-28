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

description('Tests for ES6 class constructor return values');

// ES6
// - 9.2.2 [[Construct]] (ECMAScript Function Objects)
// - 12.3.5.1 Runtime Semantics: Evaluation (The super Keyword)
// - 14.5.14 Runtime Semantics: ClassDefinitionEvaluation (Default Constructor)

var globalVariable = {name:"globalVariable"};
var globalSymbol = Symbol();

debug('Base class');
class BaseNoReturn { constructor() { } };
class BaseReturnImplicit { constructor() { return; } };
class BaseReturnUndefined { constructor() { return undefined; } };
class BaseReturnThis { constructor() { return this; } };
class BaseReturnObject { constructor() { return {a:1}; } };
class BaseReturnObject2 { constructor() { return globalVariable; } };
class BaseReturnString { constructor() { return "test"; } };
class BaseReturnNumber { constructor() { return 1; } };
class BaseReturnNull { constructor() { return null; } };
class BaseReturnSymbol { constructor() { return Symbol(); } };
class BaseThrow { constructor() { throw "Thrown Exception String"; } };

// Base - Implicit => return this.
shouldBeTrue('(new BaseNoReturn) instanceof BaseNoReturn');

// Base - Early return => return this.
shouldBeTrue('(new BaseReturnImplicit) instanceof BaseReturnImplicit');
shouldBeTrue('(new BaseReturnImplicit) !== undefined');
shouldBeTrue('(new BaseReturnUndefined) instanceof BaseReturnUndefined');
shouldBeTrue('(new BaseReturnUndefined) !== undefined');

// Base - return this => return this.
shouldBeTrue('(new BaseReturnThis) instanceof BaseReturnThis');

// Base - return Object => return object, not instance.
shouldBeFalse('(new BaseReturnObject) instanceof BaseReturnObject');
shouldBeTrue('typeof (new BaseReturnObject) === "object"');
shouldBeFalse('(new BaseReturnObject2) instanceof BaseReturnObject');
shouldBeTrue('(new BaseReturnObject2) === globalVariable');

// Base - return non-Object => return this.
shouldBeTrue('(new BaseReturnString) instanceof BaseReturnString');
shouldBeTrue('typeof (new BaseReturnString) !== "string"');
shouldBeTrue('(new BaseReturnNumber) instanceof BaseReturnNumber');
shouldBeTrue('typeof (new BaseReturnNumber) !== "number"');
shouldBeTrue('(new BaseReturnNull) instanceof BaseReturnNull');
shouldBeTrue('(new BaseReturnNull) !== null');
shouldBeTrue('(new BaseReturnSymbol) instanceof BaseReturnSymbol');
shouldBeTrue('(new BaseReturnSymbol) !== globalSymbol');

// Base - throw => throw
shouldThrow('(new BaseThrow)');

// Same behavior for Functions.
debug(''); debug('Function constructor (non-class)');
function FunctionNoReturn() { };
function FunctionReturnImplicit() { return; };
function FunctionReturnUndefined() { return undefined; };
function FunctionReturnThis() { return this; };
function FunctionReturnObject() { return {a:1}; };
function FunctionReturnObject2() { return globalVariable; };
function FunctionReturnString() { return "test"; };
function FunctionReturnNumber() { return 1; };
function FunctionReturnNull() { return null; };
function FunctionReturnSymbol() { return Symbol(); };
function FunctionThrow() { throw "Thrown Exception String"; };

shouldBeTrue('(new FunctionNoReturn) instanceof FunctionNoReturn');
shouldBeTrue('(new FunctionReturnImplicit) instanceof FunctionReturnImplicit');
shouldBeTrue('(new FunctionReturnImplicit) !== undefined');
shouldBeTrue('(new FunctionReturnUndefined) instanceof FunctionReturnUndefined');
shouldBeTrue('(new FunctionReturnUndefined) !== undefined');
shouldBeTrue('(new FunctionReturnThis) instanceof FunctionReturnThis');
shouldBeFalse('(new FunctionReturnObject) instanceof FunctionReturnObject');
shouldBeTrue('typeof (new FunctionReturnObject) === "object"');
shouldBeFalse('(new FunctionReturnObject2) instanceof FunctionReturnObject');
shouldBeTrue('(new FunctionReturnObject2) === globalVariable');
shouldBeTrue('(new FunctionReturnString) instanceof FunctionReturnString');
shouldBeTrue('typeof (new FunctionReturnString) !== "string"');
shouldBeTrue('(new FunctionReturnNumber) instanceof FunctionReturnNumber');
shouldBeTrue('typeof (new FunctionReturnNumber) !== "number"');
shouldBeTrue('(new FunctionReturnNull) instanceof FunctionReturnNull');
shouldBeTrue('(new FunctionReturnNull) !== null');
shouldBeTrue('(new FunctionReturnSymbol) instanceof FunctionReturnSymbol');
shouldBeTrue('(new FunctionReturnSymbol) !== globalSymbol');
shouldThrow('(new FunctionThrow)');


debug(''); debug('Derived class calling super()');
class DerivedNoReturn extends BaseNoReturn { constructor() { super(); } };
class DerivedReturnImplicit extends BaseNoReturn { constructor() { super(); return; } };
class DerivedReturnUndefined extends BaseNoReturn { constructor() { super(); return undefined; } };
class DerivedReturnThis extends BaseNoReturn { constructor() { super(); return this; } };
class DerivedReturnObject extends BaseNoReturn { constructor() { super(); return {a:1}; } };
class DerivedReturnObject2 extends BaseNoReturn { constructor() { super(); return globalVariable; } };
class DerivedReturnString extends BaseNoReturn { constructor() { super(); return "test"; } };
class DerivedReturnNumber extends BaseNoReturn { constructor() { super(); return 1; } };
class DerivedReturnNull extends BaseNoReturn { constructor() { super(); return null; } };
class DerivedReturnSymbol extends BaseNoReturn { constructor() { super(); return globalSymbol; } };
class DerivedThrow extends BaseNoReturn { constructor() { super(); throw "Thrown Exception String"; } };

// Derived - Implicit => return this.
shouldBeTrue('(new DerivedNoReturn) instanceof DerivedNoReturn');

// Derived - Early return => return this.
shouldBeTrue('(new DerivedReturnImplicit) instanceof DerivedReturnImplicit');
shouldBeTrue('(new DerivedReturnImplicit) !== undefined');
shouldBeTrue('(new DerivedReturnUndefined) instanceof DerivedReturnUndefined');
shouldBeTrue('(new DerivedReturnUndefined) !== undefined');

// Derived - return this => return this.
shouldBeTrue('(new DerivedReturnThis) instanceof DerivedReturnThis');

// Derived - return Object => return object, not instance.
shouldBeFalse('(new DerivedReturnObject) instanceof DerivedReturnObject');
shouldBeTrue('typeof (new DerivedReturnObject) === "object"');
shouldBeFalse('(new DerivedReturnObject2) instanceof DerivedReturnObject2');
shouldBeTrue('(new DerivedReturnObject2) === globalVariable');

// Derived - return non-Object => exception.
shouldThrow('(new DerivedReturnString)');
shouldThrow('(new DerivedReturnNumber)');
shouldThrow('(new DerivedReturnNull)');
shouldThrow('(new DerivedReturnSymbol)');
shouldThrow('(new DerivedThrow)');


debug(''); debug('Derived class not calling super()');
class DerivedNoSuperNoReturn extends BaseNoReturn { constructor() { } };
class DerivedNoSuperReturn extends BaseNoReturn { constructor() { return; } };
class DerivedNoSuperReturnUndefined extends BaseNoReturn { constructor() { return undefined; } };
class DerivedNoSuperReturnObject extends BaseNoReturn { constructor() { return {a:1}; } };
class DerivedNoSuperReturnObject2 extends BaseNoReturn { constructor() { return globalVariable; } };
class DerivedNoSuperReturnThis extends BaseNoReturn { constructor() { return this; } };
class DerivedNoSuperReturnString extends BaseNoReturn { constructor() { return "test"; } };
class DerivedNoSuperReturnNumber extends BaseNoReturn { constructor() { return 1; } };
class DerivedNoSuperReturnNull extends BaseNoReturn { constructor() { return null; } };
class DerivedNoSuperReturnSymbol extends BaseNoReturn { constructor() { return globalSymbol; } };
class DerivedNoSuperThrow extends BaseNoReturn { constructor() { throw "Thrown Exception String"; } };

// Derived without super() - Implicit => return this => TDZ.
shouldThrow('(new DerivedNoSuperNoReturn)');

// Derived without super() - Early return => return this => TDZ.
shouldThrow('(new DerivedNoSuperReturnImplicit)');
shouldThrow('(new DerivedNoSuperReturnUndefined)');

// Derived without super() - return this => return this => TDZ
shouldThrow('(new DerivedNoSuperReturnThis)');

// Derived without super() - return Object => no this access => return object, not instance
shouldNotThrow('(new DerivedNoSuperReturnObject)');
shouldNotThrow('(new DerivedNoSuperReturnObject2)');

// Derived without super() - return non-Object => exception
shouldThrow('(new DerivedNoSuperReturnString)'); // TypeError
shouldThrow('(new DerivedNoSuperReturnNumber)'); // TypeError
shouldThrow('(new DerivedNoSuperReturnNull)'); // TypeError
shouldThrow('(new DerivedNoSuperReturnSymbol)'); // TypeError
shouldThrow('(new DerivedNoSuperThrow)'); // Thrown exception


debug(''); debug('Derived class with default constructor and base class returning different values');
class DerivedDefaultConstructorWithBaseNoReturn extends BaseNoReturn { };
class DerivedDefaultConstructorWithBaseReturnImplicit extends BaseReturnImplicit { };
class DerivedDefaultConstructorWithBaseReturnUndefined extends BaseReturnUndefined { };
class DerivedDefaultConstructorWithBaseReturnThis extends BaseReturnThis { };
class DerivedDefaultConstructorWithBaseReturnObject extends BaseReturnObject { };
class DerivedDefaultConstructorWithBaseReturnObject2 extends BaseReturnObject2 { };
class DerivedDefaultConstructorWithBaseReturnString extends BaseReturnString { };
class DerivedDefaultConstructorWithBaseReturnNumber extends BaseReturnNumber { };
class DerivedDefaultConstructorWithBaseReturnNull extends BaseReturnNull { };
class DerivedDefaultConstructorWithBaseReturnSymbol extends BaseReturnSymbol { };
class DerivedDefaultConstructorWithBaseThrow extends BaseThrow { };

// Derived default constructor - implicit "super(...arguments)" return the result of the base (Object or this).
shouldBeTrue('(new DerivedDefaultConstructorWithBaseNoReturn) instanceof DerivedDefaultConstructorWithBaseNoReturn');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnImplicit) instanceof DerivedDefaultConstructorWithBaseReturnImplicit');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnUndefined) instanceof DerivedDefaultConstructorWithBaseReturnUndefined');
shouldBeFalse('(new DerivedDefaultConstructorWithBaseReturnObject) instanceof DerivedDefaultConstructorWithBaseReturnObject');
shouldBeTrue('typeof (new DerivedDefaultConstructorWithBaseReturnObject) === "object"');
shouldBeFalse('(new DerivedDefaultConstructorWithBaseReturnObject2) instanceof DerivedDefaultConstructorWithBaseReturnObject2');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnObject2) === globalVariable');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnThis) instanceof DerivedDefaultConstructorWithBaseReturnThis');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnString) instanceof DerivedDefaultConstructorWithBaseReturnString');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnNumber) instanceof DerivedDefaultConstructorWithBaseReturnNumber');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnNull) instanceof DerivedDefaultConstructorWithBaseReturnNull');
shouldBeTrue('(new DerivedDefaultConstructorWithBaseReturnSymbol) instanceof DerivedDefaultConstructorWithBaseReturnSymbol');
shouldThrow('(new DerivedDefaultConstructorWithBaseThrow)');

var successfullyParsed = true;
