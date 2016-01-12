//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var o=new Object();
o.x=5;
o.y="why";
WScript.Echo(o["x"]);
var s="y";
WScript.Echo(o[s]);
o["y"]="yes";
WScript.Echo(o.y);
for (field in o) {
   WScript.Echo(o[field]);
}

