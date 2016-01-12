//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(i)
{
    try
    {
        WScript.Echo("\t\tInner foo " + i);
        throw "thrown";
    }
    finally
    {
        WScript.Echo("\t\tFinally foo " + i);
        if (i == 0)
            return;
        else if (i == 1)
            throw 7;
    }
}

function bar(i)
{
    try
    {
        WScript.Echo("\tInner bar " + i);
        foo(i);
    }
    finally
    {
        WScript.Echo("\tFinally bar " + i);
    }
}

function foobaz(i)
{
    try
    {
        WScript.Echo("Inner foobaz " + i);
        bar(i);
    }
    catch(e)
    {
        WScript.Echo("Except foobaz " + i + " " + e);
    }
}

foobaz(0);
foobaz(1);
foobaz(2);

function testThrowInlining() {
    var y = function () {};
    Object.prototype["qfxhma"] = function qfxhma() {
        throw false;
    };
    function shapeyConstructor(waquaz) {
        qfxhma('');
        Object.defineProperty(this, "x", ({
                set :
                (function () {
                    var jqanki = y;
                })()
        }));
    }
    for (var a in[]) {
        try {
            shapeyConstructor(a);
        } catch (e) {
        }
    }
    qfxhma = y;
};
testThrowInlining();
testThrowInlining();

//Blue Bug 216103
function bar216103(a)
{
    var b=this.foo216103(a);
    return b;
}

function foo216103(a)
{
    switch(a)
    {
        case "en":
            return "english (passed)";
            break;

        case "de":
            return "german (passed)";
            break;

        default:
            throw "blah (passed)";
            break;
    }
}

try
{
    WScript.Echo(bar216103("en"));
}
catch(err)
{
    WScript.Echo(err);
}

function test() {
    var print = function () {
    };
    print(function a_indexing(fsznpi, kfoevo) {
        if (fsznpi.length == kfoevo) {
            return [eval("''++")];
        }
        var iewhao = a_indexing(fsznpi, kfoevo + 1);
        return 4;
    }([1], 0));
}
try{
  test();
}
catch(err){
  WScript.Echo(err)
};
