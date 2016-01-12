//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function()
{
    var obj0 = new( Object);
    var func1 = function(p0,p1,p2)
        {
        };
    var d = 1;
    function func4(p0,p1,p2,p3)
    {
        obj0.e %= d
    }
    var i = 0;
    do
    {
        i++;
        func1(obj0.e = ++d);
        var j = 0;
        while (d++ && j < 3)
            j++
    } while (1 && i < 3);
    WScript.Echo("obj0.e = " + (obj0.e | 0));
})()
