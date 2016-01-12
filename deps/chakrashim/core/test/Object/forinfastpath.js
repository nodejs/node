//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj = { a : 1, b: 2, c :3 };

// Test merging of string value in globopt
var c = 0;
for (var i in obj)
{
    for (var j in obj)
    {
        var temp;
        if (c < 2)
        {
            temp = i;
        }
        else
        {
            temp = j;
        }
        // Two string value merges here in globopt
        c = obj[temp];


        WScript.Echo(temp + " = " + c);

    }
}

