//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = 0;

function isless(x,y)
{
    return (x < y);
}

var i = 0;
do
{
    a += i;
    ++i;

} while(isless(i, 100) && isless(a, 5000));

WScript.Echo(a);
