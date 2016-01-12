//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var binaryOperators = ["+", "-", "*", "/", "%", ">>", ">>>", "<<", "|", "&", "^", "||", "&&"];
var unaryOperators =  ["!", "~", "-", "+" ];

var interestingTop16bits = [

    // Single bit set
    0x8000,
    0x4000,
    0x2000,
    0x1000,
    0x0800,
    0x0400,
    0x0200,
    0x0100,
    0x0080,
    0x0040,
    0x0020,
    0x0010,
    0x0008,
    0x0004,
    0x0002,
    0x0001,

    // 4 bits set
    0xf000,
    0x0f00,
    0x00f0,
    0x000f,

    // Bytes set
    0xff00,
    0x00ff,

    // Word set
    0xffff,

    // Top bit/2-bits not set
    0x7fff,
    0x3fff,
    0x7f00,
    0x3f00,
    0x007f,
    0x003f,
    0xff7f,
    0xff3f

];

function makeFloat(top16)
{
    var buf = new ArrayBuffer(8);
    var floats = new Float64Array(buf);
    var ints = new Uint16Array(buf);

    // Lower 48 bits are specific values
    ints[0] = 0xacac;
    ints[1] = 0xdd33;
    ints[2] = 0x1b2f;

    // Top 16 bits are varied.
    ints[3] = top16;

    return floats[0];
}

function hide(x)
{
    // disable inlining
    eval("");
    return x;
}

function startFunction(n)
{
    WScript.Echo("function f"+n+"() {");
}
function endFunction()
{
    WScript.Echo("}");
}

function makeTestCase(op, val1, val2)
{
    val1 = makeFloat(val1);

    if(val2 === undefined)
    {
        WScript.Echo("WScript.Echo(" + op + "  hide(" + val1 + "));");
    }
    else
    {
        val2 = makeFloat(val2);
        WScript.Echo("WScript.Echo(hide(" + val1 + ")  " + op + "  hide(" + val2 + "));");
    }
}

// Generate the test cases.
WScript.Echo(hide);

var fnc = 0;
for(var val1 in interestingTop16bits)
{
    startFunction(fnc);
    for(val2 in interestingTop16bits)
    {
        for(op in binaryOperators)
        {
            makeTestCase(binaryOperators[op], interestingTop16bits[val1], interestingTop16bits[val2]);
        }
    }
    for(op in unaryOperators)
    {
        makeTestCase(unaryOperators[op], interestingTop16bits[val1]);
    }
    endFunction();
    ++fnc;
}
for(var i = 0; i < fnc; ++i)
{
    WScript.Echo("f"+i+"();");
}
