//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var funcstr = "var t = function() { var a = 1073741823; var arr = new Array(); ";
for (var i = 0; i < 2048; i++)
{
    funcstr += " arr[0] = a * 2; ";
}
funcstr += "return arr; }";
var keep = new Array();
var scale = 1;
for (var i = 0; i < 20 * scale; i++)
{
    eval("var b = " + i  + "; " + funcstr);
    CollectGarbage();
    var ret = t();
    if (ret[0] != 2147483646) { WScript.Echo("fail"); throw 0;}
    keep.push(ret[0]);
 
    if (i % (5 * scale) == 0) 
    { 
        for (var j = 0; j < keep.length; j++)
        {
            if (keep[j] != 2147483646) { WScript.Echo("fail"); throw 1; }
        }
        keep.length = 0; 

    }
}

WScript.Echo("pass");
