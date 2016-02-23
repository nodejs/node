// Copyright 2014 the V8 project authors. All rights reserved.
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

// Flags: --harmony
'use strict';
description('Test Promise.prototype.catch.');

var reject;
var result;
var thisInInit;
var thisInOnFulfilled;

var firstPromise = new Promise(function(_, newReject) {
  thisInInit = this;
  reject = newReject;
});

var secondPromise = firstPromise.catch(function(localResult) {
  thisInOnFulfilled = this;
  shouldBe('thisInOnFulfilled', 'undefined');
  result = localResult;
  shouldBeEqualToString('result', 'hello');
  return 'bye';
});

secondPromise.then(function(localResult) {
  result = localResult;
  shouldBeEqualToString('result', 'bye');
  testPassed('fulfilled');
  finishJSTest();
}, function() {
  testFailed('rejected');
  finishJSTest();
});

shouldBe('thisInInit', 'undefined');
shouldBeTrue('firstPromise instanceof Promise');
shouldBeTrue('secondPromise instanceof Promise');

try {
  firstPromise.catch(null);
} catch (e) {
  testFailed('catch(null) should not throw an exception');
}
try {
  firstPromise.catch(37);
} catch (e) {
  testFailed('catch(37) should not throw an exception');
}

reject('hello');
