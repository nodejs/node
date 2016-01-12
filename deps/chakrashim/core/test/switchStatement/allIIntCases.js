//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
*******************************UNIT TEST FOR SWITCH CASE OPTIMIZATION*******************************
*   Testing for normal execution with all integers and bailing out
*/

/*
************************************************** TEST 1******************************************************
*   Tests with all integer arguments
*/

function f(x)
{
    switch(x)
    {
        case 1:
           WScript.Echo(1);
           break;
        case 2:
            WScript.Echo(2);
           break;
        case 3:
            WScript.Echo(3);
            break;
        case 4:
            WScript.Echo(4);
            break;
        case 5:
            WScript.Echo(5);
            break;
        case 6:
            WScript.Echo(6);
            break;
        case 7:
            WScript.Echo(7);
            break;
        case 8:
            WScript.Echo(8);
            break;
        case 9:
            WScript.Echo(9);
            break;
        case 10:
            WScript.Echo(10);
            break;
        case 11:
           WScript.Echo(11);
           break;
        case 12:
            WScript.Echo(12);
           break;
        case 13:
            WScript.Echo(13);
            break;
        case 14:
            WScript.Echo(14);
            break;
        case 15:
            WScript.Echo(15);
            break;
        case 16:
            WScript.Echo(16);
            break;
        case 17:
            WScript.Echo(17);
            break;
        case 18:
            WScript.Echo(18);
            break;
        case 19:
            WScript.Echo(19);
            break;
        case 20:
            WScript.Echo(20);
            break;
        default:
            WScript.Echo('default');
            break;
    }

}

for (i = 1; i <= 20; i++)
{
    f(i);
}

f(100);
f(0);

//making the switch to bail out on non-integer inputs
for(i=0;i<35;i++)
{
    f(new Object);
}

/*
*********************************************TEST2**********************************
*   The first execution of the switch statement is with an object
*   The profiler data is updated during the first time itself and hence the optimization is not performed during the first time jit itself
*/

function g(x)
{
    switch(x)
    {
        case 1:
           WScript.Echo(1);
           break;
        case 2:
            WScript.Echo(2);
           break;
        case 3:
            WScript.Echo(3);
            break;
        case 4:
            WScript.Echo(4);
            break;
        case 5:
            WScript.Echo(5);
            break;
        case 6:
            WScript.Echo(6);
            break;
        case 7:
            WScript.Echo(7);
            break;
        case 8:
            WScript.Echo(8);
            break;
        case 9:
            WScript.Echo(9);
            break;
        case 10:
            WScript.Echo(10);
            break;
        case 11:
           WScript.Echo(11);
           break;
        case 12:
            WScript.Echo(12);
           break;
        case 13:
            WScript.Echo(13);
            break;
        case 14:
            WScript.Echo(14);
            break;
        case 15:
            WScript.Echo(15);
            break;
        case 16:
            WScript.Echo(16);
            break;
        case 17:
            WScript.Echo(17);
            break;
        case 18:
            WScript.Echo(18);
            break;
        case 19:
            WScript.Echo(19);
            break;
        case 20:
            WScript.Echo(20);
            break;
        default:
            WScript.Echo('default');
            break;
    }

}

for(i=0;i<35;i++)
{
    g(new Object)
    g(1);
    g(-10);
    g(9);
    g(5);
}

