//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
*******************************UNIT TEST FOR SWITCH CASE OPTIMIZATION*******************************
* Test with two switch statements containing loop and retarget cases.
*/

function f(x)
{
    /* Retargetting*/
    switch(x)
    {
        case 'abc':
            break;
        case 'stu':
            break;
        default:
            WScript.Echo('Default cases');
            break;
    }

    /*Loop*/
    for(i = 0; i < 2; i++)
    {
        switch(x)
        {
            case 'abc':
                WScript.Echo('abc');
                break;
            case 'def':
                break;
            default:
                WScript.Echo('default');
                break;
        }
    }
}

f('stu');
f('stu');
f('vxy');
f('z');
f('x');
f('abc');
f('def');
f('ghi');
f('jkl');
f('mno');
f('pqr');
f('saf');
