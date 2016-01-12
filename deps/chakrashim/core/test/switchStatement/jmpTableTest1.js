//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(a)
{
    switch(a)
    {
        //JumpTable
        case 14:
            WScript.Echo('14');
            break;
        case 16:
            WScript.Echo('16');
            break;
        //Binary Tree
        case 30:
            WScript.Echo('30');
            break;
        case 40:
            WScript.Echo('40');
            break;
        case 50:
            WScript.Echo('50');
            break;

        //JumpTable
        case 61:
            WScript.Echo('61');
            break;
        case 62:
            WScript.Echo('62');
            break;

        //JumpTable
        case 1:
            WScript.Echo('1');
            break;
        case 3:
            WScript.Echo('3');
            break;

        default:
            WScript.Echo('default');
    }
}

test0(1);
test0(1);
test0(3);
test0(14);
test0(16);
test0(30);
test0(40);
test0(50);
test0(61);
test0(62);
test0(10);
