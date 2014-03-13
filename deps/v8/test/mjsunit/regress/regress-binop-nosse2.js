// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

// Flags: --allow-natives-syntax --noenable-sse2

// general tests
var e31 = Math.pow(2, 31);

assertEquals(-e31, -1*e31);
assertEquals(e31, -1*e31*(-1));
assertEquals(e31, -1*-e31);
assertEquals(e31, -e31*(-1));

var x = {toString : function() {return 1}}
function add(a,b){return a+b;}
add(1,x);
add(1,x);
%OptimizeFunctionOnNextCall(add);
add(1,x);
x.toString = function() {return "2"};

assertEquals(add(1,x), "12");

// Test the correct placement of the simulates in TruncateToNumber:
function Checker() {
  this.str = "1";
  var toStringCalled = 0;
  var toStringExpected = 0;
  this.toString = function() {
    toStringCalled++;
    return this.str;
  };
  this.check = function() {
    toStringExpected++;
    assertEquals(toStringExpected, toStringCalled);
  };
};
var left = new Checker();
var right = new Checker();

function test(fun,check_fun,a,b,does_throw) {
  left.str = a;
  right.str = b;
  try {
    assertEquals(check_fun(a,b), fun(left, right));
    assertTrue(!does_throw);
  } catch(e) {
    if (e instanceof TypeError) {
      assertTrue(!!does_throw);
    } else {
      throw e;
    }
  } finally {
    left.check();
    if (!does_throw || does_throw>1) {
      right.check();
    }
  }
}

function minus(a,b) { return a-b };
function check_minus(a,b) { return a-b };
function mod(a,b) { return a%b };
function check_mod(a,b) { return a%b };

test(minus,check_minus,1,2);
// Bailout on left
test(minus,check_minus,1<<30,1);
// Bailout on right
test(minus,check_minus,1,1<<30);
// Bailout on result
test(minus,check_minus,1<<30,-(1<<30));

// Some more interesting things
test(minus,check_minus,1,1.4);
test(minus,check_minus,1.3,4);
test(minus,check_minus,1.3,1.4);
test(minus,check_minus,1,2);
test(minus,check_minus,1,undefined);
test(minus,check_minus,1,2);
test(minus,check_minus,1,true);
test(minus,check_minus,1,2);
test(minus,check_minus,1,null);
test(minus,check_minus,1,2);
test(minus,check_minus,1,"");
test(minus,check_minus,1,2);

// Throw on left
test(minus,check_minus,{},1,1);
// Throw on right
test(minus,check_minus,1,{},2);
// Throw both
test(minus,check_minus,{},{},1);

test(minus,check_minus,1,2);

// Now with optimized code
test(mod,check_mod,1,2);
%OptimizeFunctionOnNextCall(mod);
test(mod,check_mod,1,2);

test(mod,check_mod,1<<30,1);
%OptimizeFunctionOnNextCall(mod);
test(mod,check_mod,1<<30,1);
test(mod,check_mod,1,1<<30);
%OptimizeFunctionOnNextCall(mod);
test(mod,check_mod,1,1<<30);
test(mod,check_mod,1<<30,-(1<<30));
%OptimizeFunctionOnNextCall(mod);
test(mod,check_mod,1<<30,-(1<<30));

test(mod,check_mod,1,{},2);
%OptimizeFunctionOnNextCall(mod);
test(mod,check_mod,1,{},2);

test(mod,check_mod,1,2);


// test oddballs
function t1(a, b) {return a-b}
assertEquals(t1(1,2), 1-2);
assertEquals(t1(2,true), 2-1);
assertEquals(t1(false,2), 0-2);
assertEquals(t1(1,2.4), 1-2.4);
assertEquals(t1(1.3,2.4), 1.3-2.4);
assertEquals(t1(true,2.4), 1-2.4);
assertEquals(t1(1,undefined), 1-NaN);
assertEquals(t1(1,1<<30), 1-(1<<30));
assertEquals(t1(1,2), 1-2);

function t2(a, b) {return a/b}
assertEquals(t2(1,2), 1/2);
assertEquals(t2(null,2), 0/2);
assertEquals(t2(null,-2), 0/-2);
assertEquals(t2(2,null), 2/0);
assertEquals(t2(-2,null), -2/0);
assertEquals(t2(1,2.4), 1/2.4);
assertEquals(t2(1.3,2.4), 1.3/2.4);
assertEquals(t2(null,2.4), 0/2.4);
assertEquals(t2(1.3,null), 1.3/0);
assertEquals(t2(undefined,2), NaN/2);
assertEquals(t2(1,1<<30), 1/(1<<30));
assertEquals(t2(1,2), 1/2);
