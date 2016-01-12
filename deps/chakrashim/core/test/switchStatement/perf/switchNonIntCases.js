//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
***************PERF TEST********************
* Test for cases with non-integer values.
*/
function f(x)
{
    switch(x)
    {
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        case f:break;
        default:break;
    }
}

var _switchStatementStartDate = new Date();

for(i=0;i<300000;i++)
{
    f(1)
}

var _switchStatementInterval = new Date() - _switchStatementStartDate;

WScript.Echo("### TIME:", _switchStatementInterval, "ms");

