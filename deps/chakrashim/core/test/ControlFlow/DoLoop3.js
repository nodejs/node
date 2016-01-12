//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var i = 0;
function f()
{
    WScript.Echo("condition, i = " + i);
    return i < 10;
}

WScript.Echo("--- test 1 ---");
do
{
    ++i;
    if(i > 5)
        continue;
    WScript.Echo("after continue: " + i++);
} while(f());

i = 0;

WScript.Echo("--- test 2 ---");
do
{
    switch(i++)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            continue;
        default:
            WScript.Echo("default");

        case 9:
            break;
    }
} while(f());
WScript.Echo("done");
