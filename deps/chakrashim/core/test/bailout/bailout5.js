//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var g = false;
function test(i)
{

    var a = i + 1;
    var b = a;
    // Bail out point:  test dead store of copy prop'ed values 
    if (g)
    {
        return b;
    }
    return 1;
}


WScript.Echo(test(10));
g = true;
WScript.Echo(test(10));
