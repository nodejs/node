//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//To test if BailOnNoProfile is working for MultiBr Instr created for consecutive integers
function test0(a)
{
    switch(a)
    {
        case 14:
            WScript.Echo('14');
            break;
        case 15:
            WScript.Echo('16');
            break;
        case 1:
            WScript.Echo('1');
            break;
        default:
            WScript.Echo('default');
            break;
    }
}

test0(1);
test0(14);
