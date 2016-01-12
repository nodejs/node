//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var o = new Object();
var a = [11,12,13];

a[o] = 100;
a.x  = 200;
o.x  = 300;
a.some = undefined;

write("------------ hasOwnProperty ------------");

write(o.hasOwnProperty("x"));
write(o.hasOwnProperty("y"));
write(o.hasOwnProperty(""));
write(o.hasOwnProperty());

write(a.hasOwnProperty(0));
write(a.hasOwnProperty(1));
write(a.hasOwnProperty(2));
write(a.hasOwnProperty(3));

write(a.hasOwnProperty("0"));
write(a.hasOwnProperty("1"));
write(a.hasOwnProperty("2"));
write(a.hasOwnProperty("3"));

write(a.hasOwnProperty("x"));
write(a.hasOwnProperty("some"));
write(a.hasOwnProperty("y"));
write(a.hasOwnProperty(""));

write(a.hasOwnProperty("length"));

write(a.hasOwnProperty());

write(a.hasOwnProperty(o));
write(a.hasOwnProperty("o"));
write(a.hasOwnProperty("[object Object]")); 
