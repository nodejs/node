//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }


var o;

o = Object();
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object();
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(null);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o2 = new Object(null);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(undefined);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object(undefined);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(true);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object(true);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(new Boolean(false));
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object(new Boolean(false));
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(0);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object(0);
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(new Number(10));
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object(new Number(10));
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object("hello");
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object("hello");
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

o = Object(new String("hello"));
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));
o = new Object(new String("hello"));
write("o:"  + o + " typeof(o):" + typeof(o) + " o.toString():" + Object.prototype.toString.call(o));

var b = new Boolean(true);
b.x = 10;
o = new Object(b);
write("o.x = " + o.x);

var n = new Number(100);
n.x = 20;
o = new Object(n);
write("o.x = " + o.x);

var s = new String("world");
s.x = 30;
o = new Object(s);
write("o.x = " + o.x);