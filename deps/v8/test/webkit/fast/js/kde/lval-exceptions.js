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
// Tests for raising --- and non-raising exceptions on access to reference to undefined things...

// Locals should throw on access if undefined..
fnShouldThrow(function() { a = x; }, ReferenceError);

// Read-modify-write versions of assignment should throw as well
fnShouldThrow(function() { x += "foo"; }, ReferenceError);

// Other reference types should just return undefined...
a = new Object();
fnShouldNotThrow(function() { b = a.x; });
fnShouldNotThrow(function() { b = a['x']; });
fnShouldNotThrow(function() { a['x'] += 'baz'; });
shouldBe("a['x']", '"undefinedbaz"');
fnShouldNotThrow(function() { b = a.y; });
fnShouldNotThrow(function() { a.y += 'glarch'; });
shouldBe("a['y']", '"undefinedglarch"');


// Helpers!
function fnShouldThrow(f, exType)
{
  var exception;
  var _av;
  try {
     _av = f();
  } catch (e) {
     exception = e;
  }

  if (exception) {
    if (typeof exType == "undefined" || exception instanceof exType)
      testPassed(f + " threw exception " + exception + ".");
    else
      testFailed(f + " should throw exception " + exType + ". Threw exception " + exception + ".");
  } else if (typeof _av == "undefined")
    testFailed(f + " should throw exception " + exType + ". Was undefined.");
  else
    testFailed(f + " should throw exception " + exType + ". Was " + _av + ".");
}

function fnShouldNotThrow(f)
{
  try {
    f();
    testPassed(f + " did not throw an exception");
  } catch (e) {
    testFailed(f + " threw an exception " + e + " when no exception expected");
  }
}
