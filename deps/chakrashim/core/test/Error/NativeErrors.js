//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// TrimStackTracePath is needed because same file is run in version 1/2 where LoadScriptFile is not defined
function TrimStackTracePath(obj)
{
    return obj;
}
if (this.WScript && typeof this.WScript.LoadScriptFile === "function")
{
    this.WScript.LoadScriptFile("TrimStackTracePath.js");
}

function PadString(s, l)
{
    while (s.length < l)
    {
        s += ' ';
    }
    return s;
}
function DumpObject(o)
{
    var a = new Array();
    for (var i in o)
    {
        a[a.length] = i;
    }
    a[a.length] = "description"; // Explictly adding the known non-enumerable members
    a[a.length] = "number";
    a[a.length] = "stack";
    a.sort();
    for (var i = 0; i < a.length; i++)
    {
        if (a[i] === "stack")
        {
            o[a[i]] = TrimStackTracePath(o[a[i]]);
        }
        WScript.Echo(PadString(a[i], 15) + "= " + PadString("(" + typeof(o[a[i]]) + ")", 10) + o[a[i]]);
    }
    WScript.Echo(PadString("toString()", 15) + "= " + o.toString());
}

function Test(s)
{
    WScript.Echo(s);
    DumpObject(eval("new " + s));
    WScript.Echo();
}

function safeCall(f)
{
    var args = [];
    for (var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try
    {
        return f.apply(this, args);
    }
    catch (ex)
    {
        WScript.Echo(ex.name + ": " + ex.message);
    }
}

Test("EvalError");
Test("RangeError('This is a range error')");
Test("ReferenceError");
Test("SyntaxError");
Test("TypeError('This is a type error')");
Test("URIError");
safeCall(Test, "RegExpError");
safeCall(Test, "ConversionError");
