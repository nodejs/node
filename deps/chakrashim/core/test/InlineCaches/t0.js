//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function F() {
  this.a = 1;
}

F.prototype.p1 = 10;
F.prototype.p2 = 12;
F.prototype.p3 = 13;
F.prototype.p4 = 14;
F.prototype.p5 = 15;

f = new F();

f.p1 = 20;
F.prototype.p1 = 21;
F.prototype.p5 = 22;
f.p6 = 23;
WScript.Echo(f.p1, f.p2, f.p3, f.p4, f.p5);

f = new F();
f.p2 = 24;
f.p3 = 25;
f.p5 = 26;
f.p6 = 27;
f.p4 = 28;
f.p5 = 29;
WScript.Echo(f.p1, f.p2, f.p3, f.p4, f.p5);
