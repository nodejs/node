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

description("KDE JS Test");
function kdeShouldBe(a, b, c)
{
  if ( a == b )
   debug(c+" .......... Passed");
  else
   debug(c+" .......... Failed");
}

function testThrow()
{
  var caught = false;
  try {
    throw 99;
  } catch (e) {
    caught = true;
  }
  kdeShouldBe(caught, true, "testing throw()");
}

// same as above but lacking a semicolon after throw
function testThrow2()
{
  var caught = false;
  try {
    throw 99
  } catch (e) {
    caught = true;
  }
  kdeShouldBe(caught, true, "testing throw()");
}

function testReferenceError()
{
  var err = "noerror";
  var caught = false;
  try {
    var dummy = nonexistent; // throws reference error
  } catch (e) {
    caught = true;
    err = e.name;
  }
  // test err
  kdeShouldBe(caught, true, "ReferenceError");
}

function testFunctionErrorHelper()
{
   var a = b;  // throws reference error
}

function testFunctionError()
{
  var caught = false;
  try {
    testFunctionErrorHelper();
  } catch (e) {
    caught = true;
  }
  kdeShouldBe(caught, true, "error propagation in functions");
}

function testMathFunctionError()
{
  var caught = false;
  try {
    Math();
  } catch (e) {
    debug("catch");
    caught = true;
  } finally {
    debug("finally");
  }
  kdeShouldBe(caught, true, "Math() error");
}

function testWhileAbortion()
{
  var caught = 0;
  try {
    while (a=b, 1) {         // "endless error" in condition
      ;
    }
  } catch (e) {
    caught++;
  }

  try {
    while (1) {
      var a = b;        // error in body
    }
  } catch (e) {
    caught++;
  }
  kdeShouldBe(caught, 2, "Abort while() on error");
}

debug("Except a lot of errors. They should all be caught and lead to PASS");
testThrow();
testThrow2();
testReferenceError();
testFunctionError();
testMathFunctionError();
testWhileAbortion();
