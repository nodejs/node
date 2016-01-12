//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo() {}

var oi = new Object(); oi.valueof = function() { return 100; }
var ob = new Object(); ob.valueof = function() { return false; }
var os = new Object(); os.valueof = function() { return "hello"; }

var ni = new Number(10); ni.valueof = function() { return 100; }
var nb = new Number(10); nb.valueof = function() { return false; }
var ns = new Number(10); ns.valueof = function() { return "hello"; }

var bi = new Boolean(true); bi.valueof = function() { return 100; }
var bb = new Boolean(true); bb.valueof = function() { return false; }
var bs = new Boolean(true); bs.valueof = function() { return "hello"; }

var si = new String("world"); bi.valueof = function() { return 100; }
var sb = new String("world"); bb.valueof = function() { return false; }
var ss = new String("world"); bs.valueof = function() { return "hello"; }

var all = [ undefined, null,
            true, false,
            Boolean(true), Boolean(false),
            new Boolean(true), new Boolean(false),
            NaN, +0, -0, 0, 0.0, -0.0, +0.0,
            1, 10, 10.0, 10.1, -1,
            -10, -10.0, -10.1,
            Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY,
            Number(10), Number(1), Number(0.0),
            new Number(NaN), new Number(+0), new Number(-0), new Number(0),
            new Number(0.0), new Number(-0.0), new Number(+0.0),
            new Number(1), new Number(10), new Number(10.0), new Number(10.1), new Number(-1),
            new Number(-10), new Number(-10.0), new Number(-10.1),
            new Number(Number.MAX_VALUE), new Number(Number.MIN_VALUE), new Number(Number.NaN),
            new Number(Number.POSITIVE_INFINITY), new Number(Number.NEGATIVE_INFINITY),
            "", "hello", "hel" + "lo",
            String(""), String("hello"), String("h" + "ello"),
            new String(""), new String("hello"), new String("he" + "llo"),
            new Object(), new Object(),
            [1,2,3], [1,2,3], [],
            new Array(3), Array(3), new Array(1,2,3), Array(1),
            foo, new foo(),
            oi, ob, os,
            ni, nb, ns,
            bi, bb, bs,
            si, sb, ss,
            new Date("Thu Oct 24 00:15:01 UTC+0530 1974"), new Date("Wed Oct 23 11:45:01 PDT 1974")
          ];

for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        write("a["+i+"]("+all[i]+") == a["+j+"]("+all[j]+") : " + (all[i] == all[j]));
        write("a["+i+"]("+all[i]+") != a["+j+"]("+all[j]+") : " + (all[i] != all[j]));
    }
}

