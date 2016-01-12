//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
*******************************UNIT TEST FOR SWITCH CASE OPTIMIZATION*******************************
* Test with one switch statement containing strings, objects and integers.
*/
function f(x,y)
{
    //This switch contains - a strings, a int in the middle

    switch(x)
    {
        case 'abc':
           WScript.Echo('abc');
           break;
        case 'def':
            WScript.Echo('def');
           break;
        case 'ghi':
            WScript.Echo('ghi');
            break;
        case 'jkl':
            WScript.Echo('jkl');
            break;
        case 'mno':
            WScript.Echo('mno');
            break;
        case 2:
            WScript.Echo('pqr');
            break;
        case 'stu':
            WScript.Echo('stu');
            break;
        case 'vxy':
            WScript.Echo('vxy');
            break;
        case f:
            WScript.Echo('z');
            break;
        case 'x':
            WScript.Echo('x');
            break;
        default:
            WScript.Echo('default');
            break;
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

