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
description('Test chained Promise resolutions.');

var resolve1, resolve2, resolve3;
var reject4, resolve5, resolve6;
var result;
var promise1 = new Promise(function(r) { resolve1 = r; });
var promise2 = new Promise(function(r) { resolve2 = r; });
var promise3 = new Promise(function(r) { resolve3 = r; });
var promise4 = new Promise(function(_, r) { reject4 = r; });
var promise5 = new Promise(function(r) { resolve5 = r; });
var promise6 = new Promise(function(r) { resolve6 = r; });

resolve3(promise2);
resolve2(promise1);
resolve6(promise5);
resolve5(promise4);

promise3.then(function(localResult) {
  result = localResult;
  shouldBeEqualToString('result', 'hello');
}, function() {
  testFailed('rejected');
});

promise6.then(function() {
  testFailed('fulfilled');
  finishJSTest();
}, function(localResult) {
  result = localResult;
  shouldBeEqualToString('result', 'bye');
  finishJSTest();
});

resolve1('hello');
reject4('bye');
