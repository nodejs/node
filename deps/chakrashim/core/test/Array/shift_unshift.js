//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// original
var x = [0, 1, 2, 3];
WScript.Echo(x);

x.shift();       // [1, 2, 3]
WScript.Echo(x);

x.unshift();     // [1, 2, 3]
WScript.Echo(x);

WScript.Echo(x.unshift(0));    // [0, 1, 2, 3]
WScript.Echo(x);

x.unshift("a", "b", "c"); // ["a", "b", "c", 0, 1, 2, 3]
WScript.Echo(x);

//Array subclassing
var SubArray = function()
{
};
SubArray.prototype = new Array;
var newlist = new SubArray;

//push 3 items in the subarray
newlist.push(1);
newlist.push(2);
newlist.push(3); //[1,2,3]
WScript.Echo(newlist[0]);   //1
WScript.Echo(newlist.shift());  //1
WScript.Echo(newlist.length);   //2
WScript.Echo(newlist.shift());  //2
WScript.Echo(newlist.length);   //1
WScript.Echo(newlist.shift());  //3
WScript.Echo(newlist[0]);   //undefined
WScript.Echo(newlist.length);   //0

WScript.Echo(newlist.unshift(2,3,4)); //3
WScript.Echo(newlist[0]);         //[2,3,4]
WScript.Echo(newlist.pop());          //4
WScript.Echo(newlist.unshift(5,6,7)); //[5,6,7,2,3]
WScript.Echo(newlist.length);         //5

for(i=0;i<7;i++)
    WScript.Echo(newlist.shift());

WScript.Echo(newlist.length);       //0
WScript.Echo(newlist.unshift(5,6,7));   //[5,6,7]
WScript.Echo(newlist.length);       //3
WScript.Echo(newlist.unshift());    //3

//
// A few more tests of compat and overflow
//
function echo(v) {
    WScript.Echo(v);
}

function guarded_call(func) {
    try {
        func();
    } catch (e) {
        echo(e.name + " : " + e.message);
    }
}

function dump_array(arr) {
    echo("length: " + arr.length);
    for (var p in arr) {
        if (+p == p) {
            echo("  " + p + ": " + arr[p]);
        }
    }
}

echo("--- unshift 0");
var a = [100, 101, 102];
guarded_call(function () {
    echo(a.unshift());
});
echo(a);

echo("--- unshift 1");
var a = [];
guarded_call(function () {
    var base = 4294967290;
    for (var i = 0; i < 10; i++) {
        a[base + i] = 100 + i;
    }
    delete a[base + 3];
    echo(a.unshift(200, 201, 202, 203));
});
dump_array(a);

echo("Test: unshift should throw when length is not writable."); // WOOB 1139814.
var a = {};
var origLength = 1;
Object.defineProperty(a, "length", { value: origLength, writable: false });
a[0] = 0;
try {
  Array.prototype.unshift.apply(a, ["x"]);
} catch (ex) {
  echo("e instanceOf TypeError = " + (ex instanceof TypeError));
}
echo("a.length = " + a.length); // This should be unmodified.
// Note that at this point alhough unshift throws, the array instance is modified as side-effect.
// This is according to the spec (spec bug).

//Bug 61822: array length was getting hoisted
function test0(){
  var obj1 = {};
  var func0 = function(argObj0,argMath1){
    var __loopvar4 = 0;
    do {
      if (__loopvar4 > 3) break;
      __loopvar4++;
    } while(((argObj0.length-- ) - (ary.unshift(argObj0.prop0, d, argObj0.prop2))))
  }
  Object.prototype.method0 = func0;
  var ary = new Array(10);
  var d = 1;
  obj1.method0(ary);
  WScript.Echo("ary.length = " + (ary.length|0));
};
test0();

function test1(arr)
{
    for(var __loopvar4 = 0; __loopvar4 < 2; __loopvar4++)
    {
      arr.length --;
      arr.shift();
    }
    return arr.length;
}

WScript.Echo("arr.length = " + test1(new Array(10)));

//
// To check bailouts for inlined unshift
//

var a = ['1','d','e'];
var x;
var n=1; var m = 2;
var d;
function foo()
{
    var x=2;
    var b = ['c','f'];
    a.unshift(b,x,x=1);
    a.unshift(b,x,x=2, a.unshift());
    a.unshift(b,x,x=3, a);
    a.unshift(b,x,x=4, n++, d = m+n); //n++ and d = m + n should only be calculated once
    WScript.Echo (n); //2
    WScript.Echo (d); //4
    a.unshift(b,x,x=5, d=Math.sin(n) + 1);
    WScript.Echo (x); //5
}
Array.prototype.unshift = function(){WScript.Echo ("Overriden unshift")};
foo();
WScript.Echo (a);

