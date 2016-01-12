//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var glo;
function tree1()
{
    function child1()
    {
        return tree2();
    }

    return child1();
}


function tree2()
{
    var x = 123;
    function nested()
    {
        if (doescape)
        {
            escape();
        }
        return x;
    }

    function escape()
    {
        glo = arguments.callee.caller;
    }


    return (function(param) { return param; })(nested());
}

var doescape = false;
WScript.Echo(tree1());
doescape = true;
WScript.Echo(tree1());
doescape = false;
WScript.Echo(glo());
