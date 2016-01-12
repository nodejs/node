//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v); }

Object.prototype.join = Array.prototype.join;

var n = 10;
var a = new Array();
var o = new Object();

for (var i=0; i<10; i++) {
    o[i] = a[i] = i * i + 1;
}

write(o.join());

write(o.join(undefined));

write(o.join("hello"));

write(a.join(a));
write(o.join(a));

write(a.join(o));
write(o.join(o));

write(Array.prototype.join.call(a, a));
write(Array.prototype.join.call(o, a));

write(Array.prototype.join.call(a, o));
write(Array.prototype.join.call(o, o));

//implicit calls
var a ;
var arr = [10];
Object.defineProperty(Array.prototype, "4", {configurable : true, get: function(){a = true; return 30;}});
a = false;
arr.length = 6;
var f = arr.join();
WScript.Echo(a);

Object.prototype['length'] = 2;
WScript.Echo(([""].join).call(5));
Object.prototype['0'] = "test";
WScript.Echo(([""].join).call(5.5));
