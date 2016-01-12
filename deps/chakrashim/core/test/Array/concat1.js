//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v); }
Object.prototype.concat = Array.prototype.concat;

var n = 10;
var a = new Array();
var o = new Object();

for (var i=0; i<10; i++) {
    o[i] = a[i] = i * i + 1;
}

write(a.concat());
write(o.concat());

var x = a.concat(undefined) + "";
write(x);
write(a.concat(undefined));
write(o.concat(undefined));

write(a.concat(null));
write(o.concat(null));

write(a.concat("hello"));
write(o.concat("hello"));

write(a.concat(a));
write(o.concat(a));

write(a.concat(o));
write(o.concat(o));

var b = Array.prototype.concat.call(10);
write(b[0] == 10); // true
write(b[0] === 10); // false

//implicit calls
var a ;
var arr = [10];
Object.defineProperty(Array.prototype, "4", {configurable : true, get: function(){a = true; return 30;}});
a = false;
arr.length = 6;
var f = arr.concat([30]);
WScript.Echo(a);
