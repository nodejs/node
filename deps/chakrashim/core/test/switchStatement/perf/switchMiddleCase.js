//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
***************PERF TEST********************
* Test for middle case hit with all integer case values.
*/

function f(x)
{
    switch(x)
    {
        case 1: break;
        case 2: break;
        case 3: break;
        case 4: break;
        case 5: break;
        case 6: break;
        case 7:break;
        case 8:break;
        case 9:break;
        case 10:break;
        case 11:break;
        case 12:break;
        case 13:break;
        case 14:break;
        case 15:break;
        case 16:break;
        case 17:break;
        case 18:break;
        case 19:break;
        case 20:break;
        case 21:break;
        case 22:break;
        case 23:break;
        case 24:break;
        case 25:break;
        case 26:break;
        case 27:break;
        case 28:break;
        case 29:break;
        case 30:break;
        case 31:break;
        case 32:break;
        case 33:break;
        case 34:break;
        case 35:break;
        case 36:break;
        case 37:break;
        case 38:break;
        case 39:break;
        case 40:break;
        default:break;
    }
}

var _switchStatementStartDate = new Date();

for(i=0;i<10000000;i++)
{
    f(21)
}

var _switchStatementInterval = new Date() - _switchStatementStartDate;

WScript.Echo("### TIME:", _switchStatementInterval, "ms");

