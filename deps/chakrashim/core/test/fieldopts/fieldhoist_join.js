//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(arr)
{
    var o2 = o;
    var y = 100;
    for (var i =0;i<10;i++)
    {
        y += o2.x + arr;
    }
    WScript.Echo(y);
}
var o = {x:1};
var arr = [1,2,3,4]
Array.prototype.join = function(){ return o.x+=100; }
foo(arr);
