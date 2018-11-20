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
description('Test Promise.all');

var result = undefined;

var p1 = new Promise(function(resolve) { resolve('p1'); });
var p2 = new Promise(function(resolve) { resolve('p2'); });
var p3 = new Promise(function(resolve) { resolve('p3'); });
var p4 = new Promise(function() {});
var p5 = new Promise(function() {});
var p6 = new Promise(function(_, reject) { reject('p6'); });
var p7 = new Promise(function(_, reject) { reject('p7'); });
var p8 = new Promise(function(_, reject) { reject('p8'); });
var p9 = new Promise(function(resolve) { resolve(p2); });

Promise.all([p1, p2, p5]).then(function(result) {
  testFailed('Promise.all([p1, p2, p5]) is fulfilled.');
}, function() {
  testFailed('Promise.all([p1, p2, p5]) is rejected.');
});

Promise.all().then(function() {
  testFailed('Promise.all() is fulfilled.');
}, function() {
  testPassed('Promise.all() is rejected.');
  return Promise.all([]).then(function(localResult) {
    testPassed('Promise.all([]) is fulfilled.');
    result = localResult;
    shouldBe('result.length', '0');
  }, function() {
    testFailed('Promise.all([]) is rejected.');
  });
}).then(function() {
  return Promise.all([p1, p2, p3]).then(function(localResult) {
    testPassed('Promise.all([p1, p2, p3]) is fulfilled.');
    result = localResult;
    shouldBe('result.length', '3');
    shouldBeEqualToString('result[0]', 'p1');
    shouldBeEqualToString('result[1]', 'p2');
    shouldBeEqualToString('result[2]', 'p3');
  }, function() {
    testFailed('Promise.all([p1, p2, p3]) is rejected.');
  });
}).then(function() {
  return Promise.all([p1, p6, p5]).then(function(localResult) {
    testFailed('Promise.all([p1, p6, p5]) is fulfilled.');
  }, function(localResult) {
    testPassed('Promise.all([p1, p6, p5]) is rejected.');
    result = localResult;
    shouldBeEqualToString('result', 'p6');
  });
}).then(function() {
  return Promise.all([p9]).then(function(localResult) {
    testPassed('Promise.all([p9]) is fulfilled.');
    result = localResult;
    shouldBe('result.length', '1');
    shouldBeEqualToString('result[0]', 'p2');
  }, function(result) {
    testFailed('Promise.all([p9]) is rejected.');
  });
}).then(function() {
  // Array hole should not be skipped.
  return Promise.all([p9,,,]).then(function(localResult) {
    testPassed('Promise.all([p9,,,]) is fulfilled.');
    result = localResult;
    shouldBe('result.length', '3');
    shouldBeEqualToString('result[0]', 'p2');
    shouldBe('result[1]', 'undefined');
    shouldBe('result[2]', 'undefined');
  }, function(localResult) {
    testFailed('Promise.all([p9,,,]) is rejected.');
  });
}).then(function() {
  // Immediate value should be converted to a promise object by the
  // ToPromise operation.
  return Promise.all([p9,42]).then(function(localResult) {
    testPassed('Promise.all([p9,42]) is fulfilled.');
    result = localResult;
    shouldBe('result.length', '2');
    shouldBeEqualToString('result[0]', 'p2');
    shouldBe('result[1]', '42');
  }, function(localResult) {
    testFailed('Promise.all([p9,42]) is rejected.');
  });
}).then(function() {
  return Promise.all({}).then(function(localResult) {
    testFailed('Promise.all({}) is fulfilled.');
  }, function(localResult) {
    testPassed('Promise.all({}) is rejected.');
  });
}).then(finishJSTest, finishJSTest);

shouldBe('result', 'undefined');
