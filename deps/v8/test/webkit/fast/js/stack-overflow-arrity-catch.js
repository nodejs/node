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

description('Test that if an arrity check causes a stack overflow, the exception goes to the right catch');

function funcWith20Args(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,
                        arg9, arg10, arg11, arg12, arg13, arg14, arg15,
                        arg16, arg17, arg18, arg19, arg20)
{
    debug("ERROR: Shouldn't arrive in 20 arg function!");
}

var gotRightCatch = false, gotWrongCatch1 = false, gotWrongCatch2 = false;

function test1()
{
    try {
        test2();
    } catch (err) {
        // Should get here because of stack overflow,
        // now cause a stack overflow exception due to arrity processing
        try {
            var dummy = new RegExp('a|b|c');
        } catch(err) {
            // (1) It is dendent on the stack size if we arrive here, in (2) or
            // both.
            gotWrongCatch1 = true;
        }

        try {
            funcWith20Args(1, 2, 3);
        } catch (err2) {
            gotRightCatch = true;
        }
    }
}

function test2()
{
    try {
        var dummy = new Date();
    } catch(err) {
        // (2) It is dendent on the stack size if we arrive here, in (1) or
        // both.
        gotWrongCatch2 = true;
    }

    try {
        test1();
    } catch (err) {
        // Should get here because of stack overflow,
        // now cause a stack overflow exception due to arrity processing
        try {
            funcWith20Args(1, 2, 3, 4, 5, 6);
        } catch (err2) {
            gotRightCatch = true;
        }
    }
}

test1();

shouldBeTrue("gotRightCatch");
