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

description('This test makes sure that instance of behaves correctly when the value, constructor, or its prototype are immediates.');

// A Constructor to use check for instances of, and an instance called obj.
function Constructor() {}
var obj = new Constructor();

// Run a batch of tests; call'testInstanceOf' three times, passing 1, {}, and the object 'obj', which is an instance of Constructor.
function testSet(constructor, testMethod)
{
    testMethod["1"]("(1 instanceof " + constructor + ")");
    testMethod["{}"]("({} instanceof " + constructor + ")");
    testMethod["obj"]("(obj instanceof " + constructor + ")");
}

// Test set 1, test passing the integer 1 as the constructor to be tested for.
// The constructor being an object is the first thing tested, so these should all throw.
testSet("1", { "1":shouldThrow, "{}":shouldThrow, "obj":shouldThrow });

// Test set 2, test passing an empty object ({}) as the constructor to be tested for.
// As well as being an object, the constructor must implement 'HasInstance' (i.e. be a function), so these should all throw too.
testSet("{}", { "1":shouldThrow, "{}":shouldThrow, "obj":shouldThrow });

// Test set 3, test passing Constructor as the constructor to be tested for.
// Nothing should except, the third test should pass, since obj is an instance of Constructor.
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldBeFalse, "obj":shouldBeTrue });

// Test set 4, test passing Constructor as the constructor to be tested for - with Constructor.prototype set to the integer 1.
// Constructor.prototype being a non-object will cause an exception, /unless/ value is also a non-object, since this is checked first.
Constructor.prototype = 1;
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldThrow, "obj":shouldThrow });

// Test set 5, test passing Constructor as the constructor to be tested for - with Constructor.prototype set to an empty object ({}).
// All test fail, no reason to throw.  (obj instanceof Constructor) is now false, since Constructor.prototype has changed.
Constructor.prototype = {};
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldBeFalse, "obj":shouldBeFalse });

// Test set 6, test passing Constructor as the constructor to be tested for - with Constructor.prototype set to null.
// Test that behaviour is the same as for test set 4.
Constructor.prototype = null;
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldThrow, "obj":shouldThrow });
