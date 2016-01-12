//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(v)
{
    v = v.substring(7,15);
    switch (v)
    {
        case "string0s":
            WScript.Echo("string0s");
            break;
        case "string1s":
            WScript.Echo("string1s");
            break;
        case "string2s":
            WScript.Echo("string2s");
            break;
        case "string3s":
            WScript.Echo("string3s");
            break;
        case "string4s":
            WScript.Echo("string4s");
            break;
        default:
            WScript.Echo("ERROR: " + v);
            break;
    };

}


for (var i = 0; i < 5; i++)
{
    test("prefix_string" + i + "suffix");
}
