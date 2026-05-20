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

// This neuters too low stack size passed by the flag fuzzer.
// Flags: --stack-size=864

description('Tests to ensure that Function.apply works correctly for Arrays, arguments and array-like objects.');

function argumentsApply1(a, b, c)
{
    function t(a, b, c)
    {
        return a;
    }
    return t.apply(null, arguments);
}

function argumentsApply2(a, b, c)
{
    function t(a, b, c)
    {
        return b;
    }
    return t.apply(null, arguments);
}

function argumentsApply3(a, b, c)
{
    function t(a, b, c)
    {
        return c;
    }
    return t.apply(null, arguments);
}

function argumentsApplyLength(a, b, c)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    return t.apply(null, arguments);
}
var executedAdditionalArgument = false;
function argumentsApplyExcessArguments(a, b, c)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    return t.apply(null, arguments, executedAdditionalArgument = true);
}

shouldBe("argumentsApply1(1, 2, 3)", "1");
shouldBe("argumentsApply2(1, 2, 3)", "2");
shouldBe("argumentsApply3(1, 2, 3)", "3");
shouldBe("argumentsApplyLength(1, 2, 3)", "3");
shouldBe("argumentsApplyExcessArguments(1, 2, 3)", "3");
shouldBeTrue("executedAdditionalArgument");

function arrayApply1(array)
{
    function t(a, b, c)
    {
        return a;
    }
    return t.apply(null, array);
}

function arrayApply2(array)
{
    function t(a, b, c)
    {
        return b;
    }
    return t.apply(null, array);
}

function arrayApply3(array)
{
    function t(a, b, c)
    {
        return c;
    }
    return t.apply(null, array);
}

function arrayApplyLength(array)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    return t.apply(null, array);
}

shouldBe("arrayApply1([1, 2, 3])", "1");
shouldBe("arrayApply2([1, 2, 3])", "2");
shouldBe("arrayApply3([1, 2, 3])", "3");
shouldBe("arrayApplyLength([1, 2, 3])", "3");


function argumentsApplyDelete1(a, b, c)
{
    function t(a, b, c)
    {
        return a;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

function argumentsApplyDelete2(a, b, c)
{
    function t(a, b, c)
    {
        return b;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

function argumentsApplyDelete3(a, b, c)
{
    function t(a, b, c)
    {
        return c;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

function argumentsApplyDeleteLength(a, b, c)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

shouldBe("argumentsApplyDelete1(1, 2, 3)", "1");
shouldBe("argumentsApplyDelete2(1, 2, 3)", "undefined");
shouldBe("argumentsApplyDelete3(1, 2, 3)", "3");
shouldBe("argumentsApplyDeleteLength(1, 2, 3)", "3");


function arrayApplyDelete1(array)
{
    function t(a, b, c)
    {
        return a;
    }
    delete array[1];
    return t.apply(null, array);
}

function arrayApplyDelete2(array)
{
    function t(a, b, c)
    {
        return b;
    }
    delete array[1];
    return t.apply(null, array);
}

function arrayApplyDelete3(array)
{
    function t(a, b, c)
    {
        return c;
    }
    delete array[1];
    return t.apply(null, array);
}

function arrayApplyDeleteLength(array)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    delete array[1];
    return t.apply(null, array);
}

shouldBe("arrayApplyDelete1([1, 2, 3])", "1");
shouldBe("arrayApplyDelete2([1, 2, 3])", "undefined");
shouldBe("arrayApplyDelete3([1, 2, 3])", "3");
shouldBe("arrayApplyDeleteLength([1, 2, 3])", "3");


function argumentsApplyChangeLength1()
{
    function f() {
        return arguments.length;
    };
    arguments.length = 2;
    return f.apply(null, arguments);
}


function argumentsApplyChangeLength2()
{
    function f(a) {
        return arguments.length;
    };
    arguments.length = 2;
    return f.apply(null, arguments);
}


function argumentsApplyChangeLength3()
{
    function f(a, b, c) {
        return arguments.length;
    };
    arguments.length = 2;
    return f.apply(null, arguments);
};

function argumentsApplyChangeLength4()
{
    function f() {
        return arguments.length;
    };
    arguments.length = 0;
    return f.apply(null, arguments);
};

function argumentsApplyChangeLength5()
{
    function f() {
        return arguments.length;
    };
    arguments.length = "Not A Number";
    return f.apply(null, arguments);
}

shouldBe("argumentsApplyChangeLength1(1)", "2");
shouldBe("argumentsApplyChangeLength2(1)", "2");
shouldBe("argumentsApplyChangeLength3(1)", "2");
shouldBe("argumentsApplyChangeLength4(1)", "0");
shouldBe("argumentsApplyChangeLength5(1)", "0");

function arrayApplyChangeLength1()
{
    function f() {
        return arguments.length;
    };
    var array = [];
    array.length = 2;
    return f.apply(null, array);
}

function arrayApplyChangeLength2()
{
    function f(a) {
        return arguments.length;
    };
    var array = [];
    array.length = 2;
    return f.apply(null, array);
}

function arrayApplyChangeLength3()
{
    function f(a, b, c) {
        return arguments.length;
    };
    var array = [];
    array.length = 2;
    return f.apply(null, array);
}

function arrayApplyChangeLength4()
{
    function f() {
        return arguments.length;
    };
    var array = [1];
    array.length = 0;
    return f.apply(null, array);
};

shouldBe("arrayApplyChangeLength1()", "2");
shouldBe("arrayApplyChangeLength2()", "2");
shouldBe("arrayApplyChangeLength3()", "2");
shouldBe("arrayApplyChangeLength4()", "0");

shouldBe("var a = []; a.length = 0xFFFE; [].constructor.apply('', a).length", "0xFFFE");
shouldBe("var a = []; a.length = 0xFFFF; [].constructor.apply('', a).length", "0xFFFF");
shouldBe("var a = []; a.length = 0x10000; [].constructor.apply('', a).length", "0x10000");
shouldBe("var a = []; a.length = 0x10001; [].constructor.apply('', a).length", "0x10001");
shouldThrow("var a = []; a.length = 0xFFFFFFFE; [].constructor.apply('', a).length");
shouldThrow("var a = []; a.length = 0xFFFFFFFF; [].constructor.apply('', a).length");

// ES5 permits apply with array-like objects.
shouldBe("(function(a,b,c,d){ return d ? -1 : (a+b+c); }).apply(undefined, {length:3, 0:100, 1:20, 2:3})", '123');
