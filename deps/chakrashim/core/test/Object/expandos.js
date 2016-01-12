//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v); }

var o = new Object();

o["1"] = 100; 
write("o[\"1\"]: " + o["1"]);
write("o[1]: " + o[1]);
  
o[2] = 200;
write("o[2]: " + o[2]);
write("o[\"2\"]: " + o["2"]); 

o.length = 2;
write("o.length : " + o.length);
 