//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



var o = {x: 1};

function Ctor() {};
Ctor.prototype.valueOf = function() { 
    return o.x++;
}

var c = new Ctor();

var test_less = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 1; i++)
    {
        if (o.x < c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x; 
        }
    }
    return n;
}

WScript.Echo("RESULT: " + test_less(o,c));


