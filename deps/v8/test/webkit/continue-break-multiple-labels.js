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
'This test checks break and continue behaviour in the presence of multiple labels.'
);

function test1()
{
    var s = "";

    a:
    b:
    for (var i = 1; i < 10; i++) {
       if (i == 4)
            continue a;
       s += i;
    }

    return s;
}

shouldBe("test1()", "'12356789'");

function test2()
{
    var s = "";

    a:
    b:
    for (var i = 1; i < 10; i++) {
        if (i == 4)
            break a;
        s += i;
    }

    return s;
}

shouldBe("test2()", "'123'");

function test3()
{
    var i;
    for (i = 1; i < 10; i++) {
        try {
            continue;
        } finally {
            innerLoop:
            while (1) {
                break innerLoop;
            }
        }
    }

    return i;
}

shouldBe("test3()", "10");

function test4()
{
    var i = 0;

    a:
    i++;
    while (1) {
        break;
    }

    return i;
}

shouldBe("test4()", "1");

function test5()
{
    var i = 0;

    switch (1) {
    default:
        while (1) {
            break;
        }
        i++;
    }

    return i;
}

shouldBe("test5()", "1");
