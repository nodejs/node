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

description("Ensures that we pass exceptions to the correct codeblock when throwing from the DFG to the LLInt.");
var o = {
    toString: function() { if (shouldThrow) throw {}; return ""; }
};

var shouldThrow = false;
function h(o) {
    return String(o);
}

try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}


function g() {
    with({})
        h(o);
}

function f1() {
    try {
        g();
    } catch (e) {
        testFailed("Caught exception in wrong codeblock");
    }
}

function f2() {
    try {
        g();
    } catch (e) {
        testPassed("Caught exception in correct codeblock");
    }
}

f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
shouldThrow = true;
f2();
var successfullyParsed = true;
