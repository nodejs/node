//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var global = this;
function Dump(s)
{
    var o = global[s];
    if (!o) { return; }
    WScript.Echo("for..in " + s);
    for (var i in o)
    {
        WScript.Echo("  " + i + " = " + o[i]);
    }
    WScript.Echo("for..in " + s + " (with blah)");
    o.blah = "b";
    for (var i in o)
    {
        WScript.Echo("  " + i + " = " + o[i]);
    
    }
    try
    {
        var newobj = new o();
        WScript.Echo("for..in new " + s);
        for (var i in newobj)
        {
            WScript.Echo("  " + i + " = " + newobj[i]);
    
        }

        WScript.Echo("for..in " + s + " (with prototype.blah2)");
        o.prototype.blah2 = s;
        for (var i in newobj)
        {
            WScript.Echo("  " + i + " = " + newobj[i]);
    
        }
    } 
    catch (e)
    {
    }
    WScript.Echo();
}



Dump("Object");
Dump("Array");
Dump("String");
Dump("Function");
Dump("Math");
Dump("JSON");
Dump("Number");
Dump("Boolean");
Dump("Date");
Dump("RegExp");
