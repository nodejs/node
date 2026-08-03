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
"This page tests for assertion failures in edge cases of property lookup on primitive values."
);

var didNotCrash = true;

(function () {
    delete String.prototype.constructor;
    for (var i = 0; i < 3; ++i)
        "".replace;
})();

(function () {
    String.prototype.__proto__ = { x: 1, y: 1 };
    delete String.prototype.__proto__.x;
    for (var i = 0; i < 3; ++i)
        "".y;
})();

(function () {
    function f(x) {
        x.y;
    }

    String.prototype.x = 1;
    String.prototype.y = 1;
    delete String.prototype.x;

    Number.prototype.x = 1;
    Number.prototype.y = 1;
    delete Number.prototype.x;

    for (var i = 0; i < 3; ++i)
        f("");

    for (var i = 0; i < 3; ++i)
        f(.5);
})();


var checkOkay;

function checkGet(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { get: function() { checkOkay = typeof this === 'object'; }, configurable: true });
    x.foo;
    delete constructor.prototype.foo;
    return checkOkay;
}

function checkSet(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { set: function() { checkOkay = typeof this === 'object'; }, configurable: true });
    x.foo = null;
    delete constructor.prototype.foo;
    return checkOkay;
}

function checkGetStrict(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { get: function() { "use strict"; checkOkay = typeof this !== 'object'; }, configurable: true });
    x.foo;
    delete constructor.prototype.foo;
    return checkOkay;
}

function checkSetStrict(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { set: function() { "use strict"; checkOkay = typeof this !== 'object'; }, configurable: true });
    x.foo = null;
    delete constructor.prototype.foo;
    return checkOkay;
}

shouldBeTrue("checkGet(1, Number)");
shouldBeTrue("checkGet('hello', String)");
shouldBeTrue("checkGet(true, Boolean)");
shouldBeTrue("checkSet(1, Number)");
shouldBeTrue("checkSet('hello', String)");
shouldBeTrue("checkSet(true, Boolean)");
shouldBeTrue("checkGetStrict(1, Number)");
shouldBeTrue("checkGetStrict('hello', String)");
shouldBeTrue("checkGetStrict(true, Boolean)");
shouldBeTrue("checkSetStrict(1, Number)");
shouldBeTrue("checkSetStrict('hello', String)");
shouldBeTrue("checkSetStrict(true, Boolean)");

function checkRead(x, constructor)
{
    return x.foo === undefined;
}

function checkWrite(x, constructor)
{
    x.foo = null;
    return x.foo === undefined;
}

function checkReadStrict(x, constructor)
{
    "use strict";
    return x.foo === undefined;
}

function checkWriteStrict(x, constructor)
{
    "use strict";
    x.foo = null;
    return x.foo === undefined;
}

shouldBeTrue("checkRead(1, Number)");
shouldBeTrue("checkRead('hello', String)");
shouldBeTrue("checkRead(true, Boolean)");
shouldBeTrue("checkWrite(1, Number)");
shouldBeTrue("checkWrite('hello', String)");
shouldBeTrue("checkWrite(true, Boolean)");
shouldBeTrue("checkReadStrict(1, Number)");
shouldBeTrue("checkReadStrict('hello', String)");
shouldBeTrue("checkReadStrict(true, Boolean)");
shouldThrow("checkWriteStrict(1, Number)");
shouldThrow("checkWriteStrict('hello', String)");
shouldThrow("checkWriteStrict(true, Boolean)");

function checkNumericGet(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, 42, { get: function() { checkOkay = typeof this === 'object'; }, configurable: true });
    x[42];
    delete constructor.prototype[42];
    return checkOkay;
}

function checkNumericSet(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, 42, { set: function() { checkOkay = typeof this === 'object'; }, configurable: true });
    x[42] = null;
    delete constructor.prototype[42];
    return checkOkay;
}

function checkNumericGetStrict(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, 42, { get: function() { "use strict"; checkOkay = typeof this !== 'object'; }, configurable: true });
    x[42];
    delete constructor.prototype[42];
    return checkOkay;
}

function checkNumericSetStrict(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, 42, { set: function() { "use strict"; checkOkay = typeof this !== 'object'; }, configurable: true });
    x[42] = null;
    delete constructor.prototype[42];
    return checkOkay;
}

shouldBeTrue("checkNumericGet(1, Number)");
shouldBeTrue("checkNumericGet('hello', String)");
shouldBeTrue("checkNumericGet(true, Boolean)");
shouldBeTrue("checkNumericSet(1, Number)");
shouldBeTrue("checkNumericSet('hello', String)");
shouldBeTrue("checkNumericSet(true, Boolean)");
shouldBeTrue("checkNumericGetStrict(1, Number)");
shouldBeTrue("checkNumericGetStrict('hello', String)");
shouldBeTrue("checkNumericGetStrict(true, Boolean)");
shouldBeTrue("checkNumericSetStrict(1, Number)");
shouldBeTrue("checkNumericSetStrict('hello', String)");
shouldBeTrue("checkNumericSetStrict(true, Boolean)");

function checkNumericRead(x, constructor)
{
    return x[42] === undefined;
}

function checkNumericWrite(x, constructor)
{
    x[42] = null;
    return x[42] === undefined;
}

function checkNumericReadStrict(x, constructor)
{
    "use strict";
    return x[42] === undefined;
}

function checkNumericWriteStrict(x, constructor)
{
    "use strict";
    x[42] = null;
    return x[42] === undefined;
}

shouldBeTrue("checkNumericRead(1, Number)");
shouldBeTrue("checkNumericRead('hello', String)");
shouldBeTrue("checkNumericRead(true, Boolean)");
shouldBeTrue("checkNumericWrite(1, Number)");
shouldBeTrue("checkNumericWrite('hello', String)");
shouldBeTrue("checkNumericWrite(true, Boolean)");
shouldBeTrue("checkNumericReadStrict(1, Number)");
shouldBeTrue("checkNumericReadStrict('hello', String)");
shouldBeTrue("checkNumericReadStrict(true, Boolean)");
shouldThrow("checkNumericWriteStrict(1, Number)");
shouldThrow("checkNumericWriteStrict('hello', String)");
shouldThrow("checkNumericWriteStrict(true, Boolean)");

shouldBeTrue("didNotCrash");
