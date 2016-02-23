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
"This test case checks variable resolution in the presence of both eval and with."
);

// Direct non-strict eval inside a with.

function freeVarInsideEvalAndWith(o, str)
{
    with (o)
    {
        return function () { return eval(str); }
    }
}

shouldBeTrue('freeVarInsideEvalAndWith({}, "true")()')
shouldBeFalse('freeVarInsideEvalAndWith({}, "false")()')
shouldBeTrue('freeVarInsideEvalAndWith({}, "var x = 10; x")() == 10')
shouldBeTrue('freeVarInsideEvalAndWith({}, "var x = 10; (function (){return x;})")()() == 10')

function localVarInsideEvalAndWith(o, str)
{
    with (o)
    {
        return eval(str);
    }
}

shouldBeTrue('localVarInsideEvalAndWith({}, "true")')
shouldBeFalse('localVarInsideEvalAndWith({}, "false")')
shouldBeTrue('localVarInsideEvalAndWith({}, "var x = true; x")')
shouldBeTrue('localVarInsideEvalAndWith({}, "var x = 10; (function (){return x;})")() == 10')

var y;
shouldBeTrue('localVarInsideEvalAndWith(y={x:false}, "var x = true; x && y.x")')
