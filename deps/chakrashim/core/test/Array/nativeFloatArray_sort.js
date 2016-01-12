//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a= [3.3, 2.2, 1]
Array.prototype[4] = 10;

function Test()
{
    a.sort(function(){return -1});
    WScript.Echo(a);
}

Test();

