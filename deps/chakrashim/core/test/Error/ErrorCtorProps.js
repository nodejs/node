//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var ctors = [Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError];
safeCall(eval, "ctors.push(RegExpError);");
safeCall(eval, "ctors.push(ConversionError);");

var props = ["message", "name", "description", "call", "apply"];

for (var i in ctors)
{
    Test(ctors[i]);
}

function Test(ctor)
{
    WScript.Echo("---------------------------------");
    WScript.Echo("toString(): " + ctor.toString());
    for (var j in props)
    {
        var prop = props[j];
        WScript.Echo("Property: '" + prop + "'");
        WScript.Echo("Value: '" + ctor[prop] + "'");
        WScript.Echo("hasOwnProperty: " + ctor.hasOwnProperty(prop));
    }
    WScript.Echo();
}

function safeCall(f) {
    var args = [];
    for (var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch (ex) {
        WScript.Echo(ex.name + ": " + ex.message);
    }
}
