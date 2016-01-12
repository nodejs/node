//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = [0];
a[1] = 1
a[2] = 2;
Array.prototype[3] = 3; // Should BailOut on poping this element.

a[6] = 4;

function test1()
{
    return a.pop();
}

var len = a.length;

for(i=0; i <= len; i++)
{
    WScript.Echo(test1());
}

