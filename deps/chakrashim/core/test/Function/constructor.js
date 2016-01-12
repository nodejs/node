//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo(x) {
    this.x = x;
}

var f = new foo(10);

foo.prototype = { y : 10 };

var f1 = new foo(20);

write("f : " + f.y);  // should print undefined
write("f1: " + f1.y); // should print 10


function bar(x, y) {
    this.x1 = x;
    this.x2 = x;
    this.x3 = x;
    this.x4 = x;
    this.x5 = x;
    this.x6 = x;
    this.x7 = x;
    this.x8 = x;
    this.x9 = x;
    
    this.y1 = y;
    this.y2 = y;
    this.y3 = y;
    this.y4 = y;
    this.y5 = y;
    this.y6 = y;
    this.y7 = y;
    this.y8 = y;
    this.y9 = y;
}

var b1 = new bar(10, 20);
var b2 = new bar(30, 40);

write(b2.y8);