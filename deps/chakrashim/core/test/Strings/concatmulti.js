//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var tests = [
function (str)
{
    // Can't optimize
    return "<start> " + str + " <end>";
},

function(str, str2)
{
    // Can't optimize because the first two might not be string
    return str + str2 + " something";
},

function(str, str2)
{
    // Can't optimize, side effect ordering
    return str + " something " + str2;
},

function(str, str2)
{
    // Can't optimize, side effect ordering
    return str + (" something " + str2);
},

function(str)
{
    return ("<start> " + str) 
        + (str + " <end>");
},

function(str)
{
    return "<start> " + str + str + (str += "<extra>") + " <end>";
},

function(str)
{
    return "<start> " + str + str;
}


];


function test(func, str, str2)
{
    WScript.Echo("------------------------------------------");
    WScript.Echo(func(str, str2));
    WScript.Echo(func(str, str2));
}

function alltest(str, str2)
{
    WScript.Echo("==========================================");
    WScript.Echo("Input : " + str + " | " + str2);
    WScript.Echo("==========================================");
    for (var i = 0; i < tests.length; i++)
    {
        test(tests[i], str, str2);
    } 
}

alltest("x");
alltest(12);
alltest(true);


function A() {};
function B() {};
A.prototype.toString = function() { WScript.Echo("A.toString"); return "A"; }
B.prototype.toString = function() { WScript.Echo("B.toString"); return "B"; }


alltest(new A(), new B());
