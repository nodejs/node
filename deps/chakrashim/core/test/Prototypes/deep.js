//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Creates a deep inheritance chain.

var n = 0;
var count = 30;

function Create(x)
{
    this[n++] = x;
    Create.prototype = this;
}

var a = new Array(count);

for(var i = 0; i < count; ++i)
{
    var x = new Create(i);
    a[i] = x;
}

for(var i = 0; i < count; ++i)
{
    WScript.Echo("starting " + i);

    // j <= i+1 because we intentionally access undefined properties
    for(var j = 0; j <= i+1; ++j)
    {
        WScript.Echo("" + a[i][j]);
    }
}
