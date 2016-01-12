//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
    WScript.Echo(args);
}

write("Scenario 0");
//Array sort testing to make sure no change for Strings
var s = new String("world hello");
s.foo = Array.prototype.sort;
try
{
 s.foo();
}
catch (e)
{
 if (! e instanceof TypeError)
  throw e;
 write(s);
}

//following scenario's test sparse array, prototype lookup and undefined elements
write("Scenario 1");
var a = [undefined, undefined, undefined];
a.sort();
write(a);
write(a.length);

write("Scenario 2");

var b = undefined;
var a = [b, b];
a[10] = b;
a[11] = b;
a[21] = b;
a[22] = b;
a[8] = b;
a.sort();
write(a);
write(a.length);

write("Scenario 3");
var b = undefined;
var a = [b];
a.sort();
write(a);
write(a.length);

write("Scenario 4 - prototype lookup  - output in cscript is different");

for(var i = 0;i<20;i=i+4)
{
 Object.prototype[i] = "o"+i;
}

for(var i = 0;i<20;i=i+3)
{
 Array.prototype[i] = "p"+i;
}

Array.prototype[14] = undefined;
Object.prototype[2] = undefined;

var a = [23,14, undefined, 17];

a[10] = 5;
a[11] = 22;
a[12] = undefined;
a[13] = 20;

write(a.sort());
write(a);
write(a.length);

write("Scenario 5 - prototype lookup");
var arr=new Array(3)
write(arr.sort());
write(arr);

Array.prototype[0]=0;
Array.prototype[1]=0;
Array.prototype[2]=0;

write(arr.length);

write("Scenario 6 - prototype lookup");
Array.prototype[5]=10;
Array.prototype[6]=1;
 Array.prototype[7]=15;

var arr=new Array(8)
arr[0]=1;
arr[1]=2;
arr[2]=3;
write(arr.sort());

write("Scenario 7 - output in cscript is different");

Array.prototype[5]=10;
var arr=new Array(8)
arr[1]=1;
arr[5]=undefined;
arr.sort();
write(arr)

write("Scenario 8");

Array.prototype[12]=10;
var arr=new Array(8)
arr[1]=1;
write(arr.sort());

write(arr);

function comparefn(x,y) { arr[0]="test"; return x - y; }
var arr=new Array(2);
arr[0]=12;
arr[1]=10;
arr.sort(comparefn);

write(arr);

function comparefn(x, y) { delete arr[0]; return x - y; }
var arr=new Array(3);
arr[0]=12;
arr[2]=10;
arr.sort(comparefn);

write(arr);

