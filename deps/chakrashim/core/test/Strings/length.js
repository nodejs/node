//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var print = function () { WScript.Echo.apply(this, arguments) };
try
{
    Object.prototype.createFunction=function()
    {
        print("createFunction called");
        print(this);
        return this
    }

    Object.prototype.length=function()
    {
       print("length called");
       return this
    };

    var c = "x";
    x = c.createFunction();
    x.length();
    x.length();
}
catch (e)
{
    print(e);
}
