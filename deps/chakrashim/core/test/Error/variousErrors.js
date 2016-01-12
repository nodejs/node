//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function getErrorType(e)
{
    if (e instanceof EvalError) return "EvalError";
    if (e instanceof SyntaxError) return "SyntaxError";
    if (e instanceof ReferenceError) return "ReferenceError";
    if (e instanceof TypeError) return "TypeError";
    
    return "unknown";
}

var scen = [
    "42 = 42",
    "'x' = 42",
    "true = 42",
    "null = 42",
    "delete this",
    "delete true",
    "delete 10",
    "delete null"
];

for (var i=0;i<scen.length;i++)
{
    try 
    {
        var result = eval(scen[i]);
        write(scen[i] + " .. " + result);
    }
    catch (e) 
    {
        write(scen[i] + " :: " + getErrorType(e));
    }
}
