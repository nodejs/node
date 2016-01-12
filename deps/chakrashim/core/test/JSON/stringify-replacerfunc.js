//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var a = new Object();

function replacer(k, v)
{
    return v;
}

for (var i = 0; i < 1290; i++)
{
    a[i + 10] = 0;
}

WScript.Echo(JSON.stringify(a, replacer).substring(0,20));
