//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function check(str) {
    var res = eval(str);
    write((typeof res) + " : " + res);
}

var count = 0;
function fn() { return count++;}
function fs() { count++; return (count % 2 ) ? "str1" : "str2"; }
function fb() { count++; return (count % 2 ) ? true : false;    }

var list = [ "fn", "fs", "fb" ];
var vars = [ "o", "n", "d", "a", "b"];

var o = {};
var n = new Number(123456);
var d = new Date("Thu Jan 10 05:30:01 UTC+0530 1970");
var a = [];
var b = new Boolean(true);
        
a[0] = o; a[1] = n; a[2] = d; a[3] = a; a[4] = b;
check("a.toString()");
check("a.toLocaleString()");

for (var i=0; i<list.length; i++) {
    for (var j=0; j<list.length; j++) {
        eval("o.toLocaleString = " + list[i]);
        eval("o.toString = " + list[j]);
        
        eval("n.toLocaleString = " + list[i]);
        eval("n.toString = " + list[j]);
        
        eval("d.toLocaleString = " + list[i]);
        eval("d.toString = " + list[j]);
        
        eval("b.toLocaleString = " + list[i]);
        eval("b.toString = " + list[j]);

        a[0] = o; a[1] = n; a[2] = d; a[3] = a; a[4] = b;
        
        for (var k=0; k<vars.length; k++) {
            check(vars[k] + ".toString()");
            check(vars[k] + ".toLocaleString()");
        }
    }
}

var o1 = {};
var n1 = new Number(123456);
var d1 = new Date("Thu Jan 10 05:30:01 UTC+0530 1970");
var b1 = new Boolean(true);

a[0] = o1; a[1] = n1; a[2] = d1; a[3] = a; a[4] = b1;
for (var i=0; i<list.length; i++) {
    for (var j=0; j<list.length; j++) {
        eval("a.toLocaleString = " + list[i]);
        eval("a.toString = " + list[j]);
               
        check("a.toString()");
        check("a.toLocaleString()");
    }
}