//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var d;

d = new Date("Thu Jan 10 05:30:01 UTC+0530 1970"); write(d.toISOString());
d = new Date("1974"); write(d.toISOString());
d = new Date(1974); write(d.toISOString());
d = new Date(1974, 9); write(d.toISOString());
d = new Date(1974, 9, 24); write(d.toISOString());
d = new Date(1974, 9, 24, 0); write(d.toISOString());
d = new Date(1974, 9, 24, 0, 20); write(d.toISOString());
d = new Date(1974, 9, 24, 0, 20, 30); write(d.toISOString());
d = new Date(1974, 9, 24, 0, 20, 30, 40); write(d.toISOString());
d = new Date(1974, 9, 24, 0, 20, 30, 40, 50); write(d.toISOString());
d = new Date(2000, -1200001); write(d.toISOString()); // Make sure there is no AV for negative month (WOOB 1140748).
d = new Date(2000, -1); write(d.toISOString());  // Check correctness when month is negative.
d = new Date("", 1e81); write(d); // WOOB 1139099
d = new Date(); d.setSeconds(Number.MAX_VALUE); write(d);  // WOOB 1142298
d = new Date(); d.setSeconds(-Number.MAX_VALUE); write(d); // WOOB 1142298
