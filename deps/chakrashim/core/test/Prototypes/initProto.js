//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function doEval(str)
{
    write(str + " = " + eval(str));
}

function check(type)
{
    doEval(type + ".prototype");
    doEval("typeof("+ type + ".prototype)");
    doEval(type + ".prototype.length");
    doEval("typeof("+ type + ".prototype.length)");
    doEval(type + ".prototype.toString()");
}

var types = [ 
    "Array",
    "Boolean",
    "Date",
    //"Function", // TODO: Check the function.toString
    "Number",
    "RegExp",
    "String"
]

for (var i=0; i< types.length; i++)
{
    check(types[i]);
}
