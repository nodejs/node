//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a= [1, 2.2, 3.3]
Array.prototype[4] = 10;

function Test()
{
    a.reverse();
    WScript.Echo(a);
}

Test();

