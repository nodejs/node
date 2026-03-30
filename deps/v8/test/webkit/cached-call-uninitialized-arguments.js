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
"This test checks that uninitialized parameters for cached call functions correctly defaults to undefined."

);

function doForEach(arr) {
    function callback(element, index, array, arg4, arg5, arg6) {

        function shouldBeUndefined(_a) {
            var exception;
            var _av;
            try {
                _av = eval(_a);
            } catch (e) {
                exception = e;
            }

            if (exception)
                testFailed(_a + " should be undefined. Threw exception " + exception);
            else if (typeof _av == "undefined")
                testPassed(_a + " is undefined.");
            else
                testFailed(_a + " should be undefined. Was " + _av);
        }

        shouldBeUndefined("arg4");
        shouldBeUndefined("arg5");
        shouldBeUndefined("arg6");
    }

    arr.forEach(callback);
}

function callAfterRecursingForDepth(depth, func, arr) {
    if (depth > 0) {
        callAfterRecursingForDepth(depth - 1, func, arr);
    } else {
        func(arr);
    }
}

var arr = [1];
callAfterRecursingForDepth(20, doForEach, arr);
