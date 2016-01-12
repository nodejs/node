//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var defvar = 10;
WScript.Echo(defvar);
try
{
    WScript.Echo(undefvar);
}
catch (e)
{
    WScript.Echo(e.message);
}
WScript.Echo(this.defvar);
WScript.Echo(this.undefvar);

function func()
{
    WScript.Echo(defvar);
    try
    {
        WScript.Echo(undefvar);
    }
    catch (e)
    {
        WScript.Echo(e.message);
    }

    // this refers to the global object
    WScript.Echo(this.defvar);
    WScript.Echo(this.undefvar);
    return this;
}

var g = func();

WScript.Echo(g.defvar);
WScript.Echo(g.undefvar);

