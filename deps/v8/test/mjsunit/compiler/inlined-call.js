// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   * Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above
//     copyright notice, this list of conditions and the following
//     disclaimer in the documentation and/or other materials provided
//     with the distribution.
//   * Neither the name of Google Inc. nor the names of its
//     contributors may be used to endorse or promote products derived
//     from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --noalways-opt

var global = this;

// For the HConstant
Array.prototype.fun = function() {
  funRecv = this;
  called++;
  assertEquals(0, arguments.length);
};

Array.prototype.funStrict = function() {
  "use strict";
  funStrictRecv = this;
  called++;
  assertEquals(0, arguments.length);
};

Array.prototype.manyArgs = function() {
  "use strict";
  assertEquals(5, arguments.length);
  assertEquals(0, this);
  assertEquals(5, arguments[4]);
  called++;
}

Array.prototype.manyArgsSloppy = function() {
  assertEquals(global, this);
  assertEquals(5, arguments.length);
  assertEquals(5, arguments[4]);
  called++;
}

var array = [];
for (var i = 0; i < 100; ++i) {
  array[i] = i;
}

var copy = array.slice();

function unshiftsArray(num) {
  [].unshift.call(array, num);
}

unshiftsArray(50);
unshiftsArray(60);
%OptimizeFunctionOnNextCall(unshiftsArray);
unshiftsArray(80);
unshiftsArray(50);
unshiftsArray(60);

copy.unshift(50);
copy.unshift(60);
copy.unshift(80);
copy.unshift(50);
copy.unshift(60);

assertOptimized(unshiftsArray);
assertArrayEquals(array, copy);


var called = 0;
var funRecv;

function callNoArgs() {
  [].fun.call();
}

callNoArgs();
callNoArgs();
assertEquals(this, funRecv);
%OptimizeFunctionOnNextCall(callNoArgs);
callNoArgs();
assertEquals(this, funRecv);
assertEquals(3, called);
assertOptimized(callNoArgs);

var funStrictRecv;
called = 0;

function callStrictNoArgs() {
  [].funStrict.call();
}

callStrictNoArgs();
callStrictNoArgs();
assertEquals(undefined, funStrictRecv);
%OptimizeFunctionOnNextCall(callStrictNoArgs);
callStrictNoArgs();
assertEquals(undefined, funStrictRecv);
assertEquals(3, called);
assertOptimized(callStrictNoArgs);

called = 0;


function callManyArgs() {
  [].manyArgs.call(0, 1, 2, 3, 4, 5);
}

callManyArgs();
callManyArgs();
%OptimizeFunctionOnNextCall(callManyArgs);
callManyArgs();
assertOptimized(callManyArgs);
assertEquals(called, 3);

called = 0;


function callManyArgsSloppy() {
  [].manyArgsSloppy.call(null, 1, 2, 3, 4, 5);
}

callManyArgsSloppy();
callManyArgsSloppy();
%OptimizeFunctionOnNextCall(callManyArgsSloppy);
callManyArgsSloppy();
assertOptimized(callManyArgsSloppy);
assertEquals(called, 3);

var str = "hello";
var code = str.charCodeAt(3);
called = 0;
function callBuiltinIndirectly() {
  called++;
  return "".charCodeAt.call(str, 3);
}

callBuiltinIndirectly();
callBuiltinIndirectly();
%OptimizeFunctionOnNextCall(callBuiltinIndirectly);
assertEquals(code, callBuiltinIndirectly());
assertOptimized(callBuiltinIndirectly);
assertEquals(3, called);

this.array = [1,2,3,4,5,6,7,8,9];
var copy = this.array.slice();
called = 0;

function callInlineableBuiltinIndirectlyWhileInlined() {
    called++;
    return [].push.apply(array, arguments);
}

function callInlined(num) {
  return callInlineableBuiltinIndirectlyWhileInlined(num);
}

callInlineableBuiltinIndirectlyWhileInlined(1);
callInlineableBuiltinIndirectlyWhileInlined(2);
%OptimizeFunctionOnNextCall(callInlineableBuiltinIndirectlyWhileInlined);
callInlineableBuiltinIndirectlyWhileInlined(3);
assertOptimized(callInlineableBuiltinIndirectlyWhileInlined);

callInlined(1);
callInlined(2);
%OptimizeFunctionOnNextCall(callInlined);
callInlined(3);
copy.push(1, 2, 3, 1, 2, 3);
assertOptimized(callInlined);
assertArrayEquals(copy, this.array);
assertEquals(6, called);
