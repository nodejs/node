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
"Tests loop codegen when the condition is a logical node."
);

function while_or_eq()
{
    var a = 0;
    while (a == 0 || a == 0)
        return true;
    return false;
}

shouldBeTrue("while_or_eq()");

function while_or_neq()
{
    var a = 0;
    while (a != 1 || a != 1)
        return true;
    return false;
}

shouldBeTrue("while_or_neq()");

function while_or_less()
{
    var a = 0;
    while (a < 1 || a < 1)
        return true;
    return false;
}

shouldBeTrue("while_or_less()");

function while_or_lesseq()
{
    var a = 0;
    while (a <= 1 || a <= 1)
        return true;
    return false;
}

shouldBeTrue("while_or_lesseq()");

function while_and_eq()
{
    var a = 0;
    while (a == 0 && a == 0)
        return true;
    return false;
}

shouldBeTrue("while_and_eq()");

function while_and_neq()
{
    var a = 0;
    while (a != 1 && a != 1)
        return true;
    return false;
}

shouldBeTrue("while_and_neq()");

function while_and_less()
{
    var a = 0;
    while (a < 1 && a < 1)
        return true;
    return false;
}

shouldBeTrue("while_and_less()");

function while_and_lesseq()
{
    var a = 0;
    while (a <= 1 && a <= 1)
        return true;
    return false;
}

shouldBeTrue("while_and_lesseq()");

function for_or_eq()
{
    for (var a = 0; a == 0 || a == 0; )
        return true;
    return false;
}

shouldBeTrue("for_or_eq()");

function for_or_neq()
{
    for (var a = 0; a != 1 || a != 1; )
        return true;
    return false;
}

shouldBeTrue("for_or_neq()");

function for_or_less()
{
    for (var a = 0; a < 1 || a < 1; )
        return true;
    return false;
}

shouldBeTrue("for_or_less()");

function for_or_lesseq()
{
    for (var a = 0; a <= 1 || a <= 1; )
        return true;
    return false;
}

shouldBeTrue("for_or_lesseq()");

function for_and_eq()
{
    for (var a = 0; a == 0 && a == 0; )
        return true;
    return false;
}

shouldBeTrue("for_and_eq()");

function for_and_neq()
{
    for (var a = 0; a != 1 && a != 1; )
        return true;
    return false;
}

shouldBeTrue("for_and_neq()");

function for_and_less()
{
    for (var a = 0; a < 1 && a < 1; )
        return true;
    return false;
}

shouldBeTrue("for_and_less()");

function for_and_lesseq()
{
    for (var a = 0; a <= 1 && a <= 1; )
        return true;
    return false;
}

shouldBeTrue("for_and_lesseq()");

function dowhile_or_eq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a == 0 || a == 0)
    return false;
}

shouldBeTrue("dowhile_or_eq()");

function dowhile_or_neq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a != 1 || a != 1)
    return false;
}

shouldBeTrue("dowhile_or_neq()");

function dowhile_or_less()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a < 1 || a < 1)
    return false;
}

shouldBeTrue("dowhile_or_less()");

function dowhile_or_lesseq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a <= 1 || a <= 1)
    return false;
}

shouldBeTrue("dowhile_or_lesseq()");

function dowhile_and_eq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a == 0 && a == 0)
    return false;
}

shouldBeTrue("dowhile_and_eq()");

function dowhile_and_neq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a != 1 && a != 1)
    return false;
}

shouldBeTrue("dowhile_and_neq()");

function dowhile_and_less()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a < 1 && a < 1)
    return false;
}

shouldBeTrue("dowhile_and_less()");

function dowhile_and_lesseq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a <= 1 && a <= 1)
    return false;
}

shouldBeTrue("dowhile_and_lesseq()");

function while_not_or_eq()
{
    var a = 0;
    while (!(a == 0 || a == 0))
        return true;
    return false;
}

shouldBeFalse("while_not_or_eq()");

function while_not_or_neq()
{
    var a = 0;
    while (!(a != 1 || a != 1))
        return true;
    return false;
}

shouldBeFalse("while_not_or_neq()");

function while_not_or_less()
{
    var a = 0;
    while (!(a < 1 || a < 1))
        return true;
    return false;
}

shouldBeFalse("while_not_or_less()");

function while_not_or_lesseq()
{
    var a = 0;
    while (!(a <= 1 || a <= 1))
        return true;
    return false;
}

shouldBeFalse("while_not_or_lesseq()");

function while_not_and_eq()
{
    var a = 0;
    while (!(a == 0 && a == 0))
        return true;
    return false;
}

shouldBeFalse("while_not_and_eq()");

function while_not_and_neq()
{
    var a = 0;
    while (!(a != 1 && a != 1))
        return true;
    return false;
}

shouldBeFalse("while_not_and_neq()");

function while_not_and_less()
{
    var a = 0;
    while (!(a < 1 && a < 1))
        return true;
    return false;
}

shouldBeFalse("while_not_and_less()");

function while_not_and_lesseq()
{
    var a = 0;
    while (!(a <= 1 && a <= 1))
        return true;
    return false;
}

shouldBeFalse("while_not_and_lesseq()");

function for_not_or_eq()
{
    for (var a = 0; !(a == 0 || a == 0); )
        return true;
    return false;
}

shouldBeFalse("for_not_or_eq()");

function for_not_or_neq()
{
    for (var a = 0; !(a != 1 || a != 1); )
        return true;
    return false;
}

shouldBeFalse("for_not_or_neq()");

function for_not_or_less()
{
    for (var a = 0; !(a < 1 || a < 1); )
        return true;
    return false;
}

shouldBeFalse("for_not_or_less()");

function for_not_or_lesseq()
{
    for (var a = 0; !(a <= 1 || a <= 1); )
        return true;
    return false;
}

shouldBeFalse("for_not_or_lesseq()");

function for_not_and_eq()
{
    for (var a = 0; !(a == 0 && a == 0); )
        return true;
    return false;
}

shouldBeFalse("for_not_and_eq()");

function for_not_and_neq()
{
    for (var a = 0; !(a != 1 && a != 1); )
        return true;
    return false;
}

shouldBeFalse("for_not_and_neq()");

function for_not_and_less()
{
    for (var a = 0; !(a < 1 && a < 1); )
        return true;
    return false;
}

shouldBeFalse("for_not_and_less()");

function for_not_and_lesseq()
{
    for (var a = 0; !(a <= 1 && a <= 1); )
        return true;
    return false;
}

shouldBeFalse("for_not_and_lesseq()");

function dowhile_not_or_eq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a == 0 || a == 0))
    return false;
}

shouldBeFalse("dowhile_not_or_eq()");

function dowhile_not_or_neq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a != 1 || a != 1))
    return false;
}

shouldBeFalse("dowhile_not_or_neq()");

function dowhile_not_or_less()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a < 1 || a < 1))
    return false;
}

shouldBeFalse("dowhile_not_or_less()");

function dowhile_not_or_lesseq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a <= 1 || a <= 1))
    return false;
}

shouldBeFalse("dowhile_not_or_lesseq()");

function dowhile_not_and_eq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a == 0 && a == 0))
    return false;
}

shouldBeFalse("dowhile_not_and_eq()");

function dowhile_not_and_neq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a != 1 && a != 1))
    return false;
}

shouldBeFalse("dowhile_not_and_neq()");

function dowhile_not_and_less()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a < 1 && a < 1))
    return false;
}

shouldBeFalse("dowhile_not_and_less()");

function dowhile_not_and_lesseq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (!(a <= 1 && a <= 1))
    return false;
}

shouldBeFalse("dowhile_not_and_lesseq()");

function float_while_or_eq()
{
    var a = 0.1;
    while (a == 0.1 || a == 0.1)
        return true;
    return false;
}

shouldBeTrue("float_while_or_eq()");

function float_while_or_neq()
{
    var a = 0.1;
    while (a != 1.1 || a != 1.1)
        return true;
    return false;
}

shouldBeTrue("float_while_or_neq()");

function float_while_or_less()
{
    var a = 0.1;
    while (a < 1.1 || a < 1.1)
        return true;
    return false;
}

shouldBeTrue("float_while_or_less()");

function float_while_or_lesseq()
{
    var a = 0.1;
    while (a <= 1.1 || a <= 1.1)
        return true;
    return false;
}

shouldBeTrue("float_while_or_lesseq()");

function float_while_and_eq()
{
    var a = 0.1;
    while (a == 0.1 && a == 0.1)
        return true;
    return false;
}

shouldBeTrue("float_while_and_eq()");

function float_while_and_neq()
{
    var a = 0.1;
    while (a != 1.1 && a != 1.1)
        return true;
    return false;
}

shouldBeTrue("float_while_and_neq()");

function float_while_and_less()
{
    var a = 0.1;
    while (a < 1.1 && a < 1.1)
        return true;
    return false;
}

shouldBeTrue("float_while_and_less()");

function float_while_and_lesseq()
{
    var a = 0.1;
    while (a <= 1.1 && a <= 1.1)
        return true;
    return false;
}

shouldBeTrue("float_while_and_lesseq()");

function float_for_or_eq()
{
    for (var a = 0.1; a == 0.1 || a == 0.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_or_eq()");

function float_for_or_neq()
{
    for (var a = 0.1; a != 1.1 || a != 1.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_or_neq()");

function float_for_or_less()
{
    for (var a = 0.1; a < 1.1 || a < 1.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_or_less()");

function float_for_or_lesseq()
{
    for (var a = 0.1; a <= 1.1 || a <= 1.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_or_lesseq()");

function float_for_and_eq()
{
    for (var a = 0.1; a == 0.1 && a == 0.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_and_eq()");

function float_for_and_neq()
{
    for (var a = 0.1; a != 1.1 && a != 1.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_and_neq()");

function float_for_and_less()
{
    for (var a = 0.1; a < 1.1 && a < 1.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_and_less()");

function float_for_and_lesseq()
{
    for (var a = 0.1; a <= 1.1 && a <= 1.1; )
        return true;
    return false;
}

shouldBeTrue("float_for_and_lesseq()");

function float_dowhile_or_eq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a == 0.1 || a == 0.1)
    return false;
}

shouldBeTrue("float_dowhile_or_eq()");

function float_dowhile_or_neq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a != 1.1 || a != 1.1)
    return false;
}

shouldBeTrue("float_dowhile_or_neq()");

function float_dowhile_or_less()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a < 1.1 || a < 1.1)
    return false;
}

shouldBeTrue("float_dowhile_or_less()");

function float_dowhile_or_lesseq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a <= 1.1 || a <= 1.1)
    return false;
}

shouldBeTrue("float_dowhile_or_lesseq()");

function float_dowhile_and_eq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a == 0.1 && a == 0.1)
    return false;
}

shouldBeTrue("float_dowhile_and_eq()");

function float_dowhile_and_neq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a != 1.1 && a != 1.1)
    return false;
}

shouldBeTrue("float_dowhile_and_neq()");

function float_dowhile_and_less()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a < 1.1 && a < 1.1)
    return false;
}

shouldBeTrue("float_dowhile_and_less()");

function float_dowhile_and_lesseq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (a <= 1.1 && a <= 1.1)
    return false;
}

shouldBeTrue("float_dowhile_and_lesseq()");

function float_while_not_or_eq()
{
    var a = 0.1;
    while (!(a == 0.1 || a == 0.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_or_eq()");

function float_while_not_or_neq()
{
    var a = 0.1;
    while (!(a != 1.1 || a != 1.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_or_neq()");

function float_while_not_or_less()
{
    var a = 0.1;
    while (!(a < 1.1 || a < 1.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_or_less()");

function float_while_not_or_lesseq()
{
    var a = 0.1;
    while (!(a <= 1.1 || a <= 1.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_or_lesseq()");

function float_while_not_and_eq()
{
    var a = 0.1;
    while (!(a == 0.1 && a == 0.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_and_eq()");

function float_while_not_and_neq()
{
    var a = 0.1;
    while (!(a != 1.1 && a != 1.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_and_neq()");

function float_while_not_and_less()
{
    var a = 0.1;
    while (!(a < 1.1 && a < 1.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_and_less()");

function float_while_not_and_lesseq()
{
    var a = 0.1;
    while (!(a <= 1.1 && a <= 1.1))
        return true;
    return false;
}

shouldBeFalse("float_while_not_and_lesseq()");

function float_for_not_or_eq()
{
    for (var a = 0.1; !(a == 0.1 || a == 0.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_or_eq()");

function float_for_not_or_neq()
{
    for (var a = 0.1; !(a != 1.1 || a != 1.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_or_neq()");

function float_for_not_or_less()
{
    for (var a = 0.1; !(a < 1.1 || a < 1.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_or_less()");

function float_for_not_or_lesseq()
{
    for (var a = 0.1; !(a <= 1.1 || a <= 1.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_or_lesseq()");

function float_for_not_and_eq()
{
    for (var a = 0.1; !(a == 0.1 && a == 0.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_and_eq()");

function float_for_not_and_neq()
{
    for (var a = 0.1; !(a != 1.1 && a != 1.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_and_neq()");

function float_for_not_and_less()
{
    for (var a = 0.1; !(a < 1.1 && a < 1.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_and_less()");

function float_for_not_and_lesseq()
{
    for (var a = 0.1; !(a <= 1.1 && a <= 1.1); )
        return true;
    return false;
}

shouldBeFalse("float_for_not_and_lesseq()");

function float_dowhile_not_or_eq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a == 0.1 || a == 0.1))
    return false;
}

shouldBeFalse("float_dowhile_not_or_eq()");

function float_dowhile_not_or_neq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a != 1.1 || a != 1.1))
    return false;
}

shouldBeFalse("float_dowhile_not_or_neq()");

function float_dowhile_not_or_less()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a < 1.1 || a < 1.1))
    return false;
}

shouldBeFalse("float_dowhile_not_or_less()");

function float_dowhile_not_or_lesseq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a <= 1.1 || a <= 1.1))
    return false;
}

shouldBeFalse("float_dowhile_not_or_lesseq()");

function float_dowhile_not_and_eq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a == 0.1 && a == 0.1))
    return false;
}

shouldBeFalse("float_dowhile_not_and_eq()");

function float_dowhile_not_and_neq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a != 1.1 && a != 1.1))
    return false;
}

shouldBeFalse("float_dowhile_not_and_neq()");

function float_dowhile_not_and_less()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a < 1.1 && a < 1.1))
    return false;
}

shouldBeFalse("float_dowhile_not_and_less()");

function float_dowhile_not_and_lesseq()
{
    var a = 0.1;
    var i = 0.1;
    do {
        if (i > 0.1)
            return true;
        i++;
    } while (!(a <= 1.1 && a <= 1.1))
    return false;
}
