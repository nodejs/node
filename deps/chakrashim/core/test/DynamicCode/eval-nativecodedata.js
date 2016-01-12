//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------




var f; 

for (var j = 0; j < 1000;j ++)
{
    eval("f = function(o) { o.a += " + j + "; for (var i = 0; i < 2; i++) { o.a += i; }}");
    for (var i = 0; i < 10; i++)
    {
        var o = { a: 0 };
        f(o);
        if (o.a != j + 1)
        {
            WScript.Echo("Failed " + j + " = " + o.a);
        }
    }
    f = null;
    CollectGarbage();

}

f = null;
CollectGarbage();
