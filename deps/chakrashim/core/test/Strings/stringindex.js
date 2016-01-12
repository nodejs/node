//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(args);
}

write("Scenario 1");

var sl = "hello";

for (var i=0;i<8;i++)
{
 write(sl.propertyIsEnumerable(i));
 write(sl.hasOwnProperty(i));
 write(sl[i]);
}

write("Scenario 2");
var so = new String("hello");
so[1] = 10;
so[4] = 20;
so[7] = 30;
so.x = 20;

for (var i=0;i<8;i++)
{
 write(so.propertyIsEnumerable(1));
 write(so.hasOwnProperty(i));
 write(so[i]);
}
write(so.propertyIsEnumerable("x"));
write(so.hasOwnProperty("x"));
write(so["x"]);