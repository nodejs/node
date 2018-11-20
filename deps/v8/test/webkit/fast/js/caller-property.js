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
'This tests for caller property in functions. Only functions that are called from inside of other functions and have a parent should have this property set. Tests return true when caller is found and false when the caller is null.'
)
function child()
{
    return (child.caller !== null);
}

function parent()
{
    return child();
}

var childHasCallerWhenExecutingGlobalCode = (child.caller !== null);
var childHasCallerWhenCalledWithoutParent = child();
var childHasCallerWhenCalledFromWithinParent = parent();

shouldBe('childHasCallerWhenExecutingGlobalCode', 'false');
shouldBe('childHasCallerWhenCalledWithoutParent', 'false');
shouldBe('childHasCallerWhenCalledFromWithinParent', 'true')

// The caller property should throw in strict mode, and a non-strict function cannot use caller to reach a strict caller (see ES5.1 15.3.5.4).
function nonStrictCallee() { return nonStrictCallee.caller; }
function strictCallee() { "use strict"; return strictCallee.caller; }
function nonStrictCaller(x) { return x(); }
function strictCaller(x) { "use strict"; return x(); }
shouldBe("nonStrictCaller(nonStrictCallee)", "nonStrictCaller");
shouldThrow("nonStrictCaller(strictCallee)", '"TypeError: Type error"');
shouldThrow("strictCaller(nonStrictCallee)", '"TypeError: Function.caller used to retrieve strict caller"');
shouldThrow("strictCaller(strictCallee)", '"TypeError: Type error"');

// .caller within a bound function reaches the caller, ignoring the binding.
var boundNonStrictCallee = nonStrictCallee.bind();
var boundStrictCallee = strictCallee.bind();
shouldBe("nonStrictCaller(boundNonStrictCallee)", "nonStrictCaller");
shouldThrow("nonStrictCaller(boundStrictCallee)", '"TypeError: Type error"');
shouldThrow("strictCaller(boundNonStrictCallee)", '"TypeError: Function.caller used to retrieve strict caller"');
shouldThrow("strictCaller(boundStrictCallee)", '"TypeError: Type error"');

// Check that .caller works (or throws) as expected, over an accessor call.
function getFooGetter(x) { return Object.getOwnPropertyDescriptor(x, 'foo').get; }
function getFooSetter(x) { return Object.getOwnPropertyDescriptor(x, 'foo').set; }
var nonStrictAccessor = {
    get foo() { return getFooGetter(nonStrictAccessor).caller; },
    set foo(x) { if (getFooSetter(nonStrictAccessor).caller !==x) throw false; }
};
var strictAccessor = {
    get foo() { "use strict"; return getFooGetter(strictAccessor).caller; },
    set foo(x) { "use strict"; if (getFooSetter(strictAccessor).caller !==x) throw false; }
};
function nonStrictGetter(x) { return x.foo; }
function nonStrictSetter(x) { x.foo = nonStrictSetter; return true; }
function strictGetter(x) { "use strict"; return x.foo; }
function strictSetter(x) { "use strict"; x.foo = nonStrictSetter; return true; }
shouldBe("nonStrictGetter(nonStrictAccessor)", "nonStrictGetter");
shouldBeTrue("nonStrictSetter(nonStrictAccessor)");
shouldThrow("nonStrictGetter(strictAccessor)", '"TypeError: Type error"');
shouldThrow("nonStrictSetter(strictAccessor)", '"TypeError: Type error"');
shouldThrow("strictGetter(nonStrictAccessor)", '"TypeError: Function.caller used to retrieve strict caller"');
shouldThrow("strictSetter(nonStrictAccessor)", '"TypeError: Function.caller used to retrieve strict caller"');
shouldThrow("strictGetter(strictAccessor)", '"TypeError: Type error"');
shouldThrow("strictSetter(strictAccessor)", '"TypeError: Type error"');
