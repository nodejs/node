//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var p = /(\d*)(\D*)/g;
var s = "123!234!567";

while (true)
{
    var r = p.exec(s);
    WScript.Echo("result=" + r);
    WScript.Echo("p.lastIndex=" + p.lastIndex);
    if (r[0].length == 0) { break; }
}
