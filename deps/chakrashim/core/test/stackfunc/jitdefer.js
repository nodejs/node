//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function foo(param)
{
    var x = 100;
    function bar()
    {
        WScript.Echo(x);
    }
    function baz(param1)
    {
        function inner(){
            count++;
            WScript.Echo("COUNT:  " + count);
            bar();
            o["i"] = arguments["callee"];

        }
        if(param1)
            return inner()
        else
            return bar();
    }
    if(param)
        return baz(true);
    else
        return baz(false);
}
var count = 0;
var o = {};
WScript.Echo(foo(false));
WScript.Echo(foo(true));
WScript.Echo(o.i());




