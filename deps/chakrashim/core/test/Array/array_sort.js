//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(args);
}

//Array sort testing for Array type
var x = [120, 5, 8, 4, 6, 9, 9, 10, 2, 3];


function c(a,b) {return a - b}

write(x.sort());

write(x.sort(c));


//Sort numerically and ascending:
var myarray=[25, 8, 7, 41]
write(myarray.sort(function(a,b){return a - b}))

//Sort numerically and descending:
var myarray2=[25, 8, 7, 41]
write(myarray.sort(function(a,b){return b - a})) //Array now becomes [41, 25, 8, 71


var mystr = new Array("some", "sample", "strings", "for", "testing");

write(mystr.sort());
write(mystr.sort(function(a,b){return a - b}) + " - Output different in cscript due to NaN");

var a;

function setup(size) {
   var i;
   a=new Array();
   for (i=0;i<size;i++)
     a[a.length]=(size/2)-i;
}

function numeric(a,b) {return a - b}

function test() {
   a.sort(numeric);
}

setup(10);
test();
setup(100);
test();
setup(1000);
test();


function compare(a,b)
{
  this.xyz = 10;
  return a - b;
}

var testThis = [1,2,3];
write(testThis.sort(compare));
write(xyz);

a = [ 1, 1.2, 12, 4.8, 4 ];
write(a.sort(function(x, y) { return x - y }));

a = [3, 2, 1];
try
{
    // throws
    a.sort(null);
} catch (e) {
    write(e);
}

// default comparer
a.sort(undefined);

write(a);
