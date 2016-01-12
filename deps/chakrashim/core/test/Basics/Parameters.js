//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function dump(a)
{
    if (a == null)
    {
        WScript.Echo("'null'");
    }
    else if (a == undefined)
    {
        WScript.Echo("'undefined'");
    }
    else
    {
        WScript.Echo(a);
    }
}


function f1(a, b)
{
    // TODO: Dump arguments.length, argument entries, etc.

    dump("f1(a, b)");
    dump(a);
    dump(b);
}


// Correct number of parameters
f1(1, 2);

// Extra parameters
f1(1, 2, 3, 4);

// Not enough parameters
f1(1);
