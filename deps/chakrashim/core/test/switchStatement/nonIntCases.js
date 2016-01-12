//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
******************************UNIT TEST FOR SWITCH CASE OPTIMIZATION*****************************
*/

/*
************************************************ TEST 1**********************************************
***Test - all the case statements containing non-integer items
*/
function f(x)
{
    switch(x)
    {
        case f:
           WScript.Echo(1);
           break;
        case f:
            WScript.Echo(2);
           break;
        case f:
            WScript.Echo(3);
            break;
        case f:
            WScript.Echo(4);
            break;
        case f:
            WScript.Echo(5);
            break;
        case f:
            WScript.Echo(6);
            break;
        case f:
            WScript.Echo(7);
            break;
        case f:
            WScript.Echo(8);
            break;
        case f:
            WScript.Echo(9);
            break;
        case f:
            WScript.Echo(10);
            break;
        default:
            WScript.Echo('first switch default');
            break;

    }

}

for(i=0;i<5;i++)
{
    f(11);
}

/*
************************************************ TEST 2**********************************************
*Test with mixed type in case statements - Integers, objects, and expressions
*/
function g(x)
{
    switch(x)
    {
        case f:
           WScript.Echo(1);
           break;
        case 2:
            WScript.Echo(2);
           break;
        case f:
            WScript.Echo(3);
            break;
        case 4:
            WScript.Echo(4);
            break;
        case 'hello':
            WScript.Echo('hello');
            break;
        case 5:
            WScript.Echo(5);
            break;
        case f:
            WScript.Echo('f');
            break;
        case 6:
            WScript.Echo(6);
            break;
        case 7:
            WScript.Echo(7);
            break;
        case 7+5:
            WScript.Echo(13);
            break;
        case 8:
            WScript.Echo(8);
            break;
        default:
            WScript.Echo('second switch default');
            break;
    }

}

g(1);
g(2);
g(3);
g(8);
g(5);
g(13);
g(new Object)

