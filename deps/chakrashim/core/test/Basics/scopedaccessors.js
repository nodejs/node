//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("test1: nested setter without getter");

function top1() {
    var xx = new Object();
    Object.defineProperty(xx, "yy", { set: function(val) {WScript.Echo("in nested setter1"); this.val = 10;} });
    var z = function() {
       xx.yy = 20;
       WScript.Echo(xx.yy);
    }
    return z;
}

var foo = top1();
foo();
WScript.Echo("test2: nested setter and setter");
function top2() {
    var xx = new Object();
    Object.defineProperty(xx, "yy", { get: function() { return this; },
    set: function(val) {WScript.Echo("in nested setter2"); this.val = 11;} });
    var z = function() {
       xx.yy = 20;
       WScript.Echo(xx.yy);
       WScript.Echo(xx.yy.val);
    }
    return z;
}

var foo2 = top2();
foo2();

WScript.Echo("test3: nested setter and setter from this");
function top3() {
    Object.defineProperty(this, "yy", { get: function() { return this; },
    set: function(val) {WScript.Echo("in nested setter3"); this.val = 12;} });
    var z = function() {
       yy = 20;
       WScript.Echo(yy);
       WScript.Echo(yy.val);    }
    return z;
}

var foo3 = top3();
foo3();

WScript.Echo("test4: closure and with");

var withObj = new Object();
Object.defineProperty(withObj, "tt", { get: function() { return this; },
    set: function(val) {WScript.Echo("in nested setter3"); this.val = 13;} });

function top4(inVar) {
    with (inVar)
    {
    Object.defineProperty(this, "tt", { get: function() { return this; },
    set: function(val) {WScript.Echo("in nested setter3"); this.val = 14;} });
    var z = function() {
       tt = 20;
       WScript.Echo(tt);
       WScript.Echo(tt.val);    }
    return z;
    }
}

var foo4 = top4(withObj);
foo4();
