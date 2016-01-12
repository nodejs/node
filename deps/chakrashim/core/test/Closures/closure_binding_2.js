//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Scenario: testing binding to the closure");

function f()
{
        x = 12;

        this.get = function()
        {
                WScript.Echo("x = " + x);
                return x;
        }

        this.set = function(n)
        {
                WScript.Echo("Setting x to " + n);
                x = n;
        }

        // Declare the variable at the lexical end of the scope to
        // make the binding tougher for the byte code generator.
        var x;
}

var x = new f();
var y = new f();

for(i = 0; i < 4; ++i)
{
        x.set(i);
        y.set(i+100);
        x.get();
        y.get();
}
