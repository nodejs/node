//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var d;

d = Date.UTC("1974"); write(d);
d = Date.UTC(1974); write(d);
d = Date.UTC(1974, 9); write(d);
d = Date.UTC(1974, 9, 24); write(d);
d = Date.UTC(1974, 9, 24, 0); write(d);
d = Date.UTC(1974, 9, 24, 0, 20); write(d);
d = Date.UTC(1974, 9, 24, 0, 20, 30); write(d);
d = Date.UTC(1974, 9, 24, 0, 20, 30, 40); write(d);
d = Date.UTC(1974, 9, 24, 0, 20, 30, 40, 50); write(d);
d = Date.UTC(1, 9, 24, 0, 20, 30, 40); write(d);
d = Date.UTC(74, 9, 24, 0, 20, 30, 40, 50); write(d);
d = Date.UTC("hello"); write(d);
d = Date.UTC(); write(d);
