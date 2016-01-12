//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function showProperty(x, y)
{
    var value = y[x];
    if (x === "stack") {
        value = value && value.replace(/\(.+\\test.Error./ig, "(");
    }
    WScript.Echo("    " + x + "\t  isOwn = " + y.hasOwnProperty(x) + "\t value = " + value);
}

function test(y)
{
    WScript.Echo("  ToString = "+y);
    WScript.Echo("  Properties = ");
    showProperty("name", y);
    showProperty("message", y);
    showProperty("stack", y); // Explictly adding the known non-enumerable members
    showProperty("number", y);
    showProperty("description", y);
    for (x in y)
    {
        showProperty(x, y);
    }
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

WScript.Echo("Error.prototype");
test(Error.prototype);

WScript.Echo("RangeError.prototype");
test(RangeError.prototype);

safeCall(function () {
    WScript.Echo("ConversionError.prototype");
    test(ConversionError.prototype);
});

WScript.Echo("\nError");
test(Error) ;

err = new Error();
WScript.Echo("\nnew Error()");
test(err);

err = new Error(undefined);
WScript.Echo("\nnew Error(undefined)");
test(err);

err = new Error(null);
WScript.Echo("\nnew Error(null)");
test(err);

err = new Error("Hello");
WScript.Echo("\nnew Error(\"Hello\")");
test(err);

err = new Error(100, "With a number");
WScript.Echo("\nnew Error(100, \"With a number\")");
test(err) ;

err = new Error("Hello");
err.name = undefined;
WScript.Echo("\nnew Error(\"Hello\"); name=undefined");
test(err);

err = new ReferenceError("I'm a reference error");
WScript.Echo("\nnew ReferenceError(\"I'm a reference error\")");
test(err);

safeCall(function () {
    err = new RegExpError(22, "This is a RegExp error");
    WScript.Echo("\nnew RegExpError(22, \"This is a RegExp error\")");
    test(err);
});

err = new TypeError();
WScript.Echo("\nnew TypeError()");
test(err);

err = new TypeError(undefined);
WScript.Echo("\nnew TypeError(undefined)");
test(err);

err = new TypeError(null);
WScript.Echo("\nnew TypeError(null)");
test(err);

var undef;
err = new TypeError("With a undef name");
err.name = undef;
WScript.Echo("\nnew TypeError(\"With a undef name\")");
test(err);

WScript.Echo("\nRuntime TypeError()");
try
{
    blah = boo;
} catch(err)
{
    test(err);
}
