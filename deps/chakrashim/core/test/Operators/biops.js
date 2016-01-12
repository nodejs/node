//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo() {}
var d = new Date("Thu Aug 5 05:30:00 PDT 2010");

var all = [ undefined, null,
            true, false,
            Boolean(true), Boolean(false),
            new Boolean(true), new Boolean(false),
            NaN, +0, -0, 0, 0.0, -0.0, +0.0,
            1, 10, 10.0, 10.1, -1, 
            -10, -10.0, -10.1,
            Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY,
            new Number(NaN), new Number(+0), new Number(-0), new Number(0), 
            new Number(0.0), new Number(-0.0), new Number(+0.0), 
            new Number(1), new Number(10), new Number(10.0), new Number(10.1), new Number(-1), 
            new Number(-10), new Number(-10.0), new Number(-10.1),
            new Number(Number.MAX_VALUE), new Number(Number.MIN_VALUE), new Number(Number.NaN), 
            new Number(Number.POSITIVE_INFINITY), new Number(Number.NEGATIVE_INFINITY),
            "", "0xa", "04", "hello", "hel" + "lo",
            String(""), String("hello"), String("h" + "ello"),
            new String(""), new String("hello"), new String("he" + "llo"),
            new Object(), new Object(),
            [1,2,3], [1,2,3],
            new Array(3), Array(3), new Array(1, 2, 3), Array(1),
            foo, d, 1281011400000 , d.getVarDate()

          ];

var biops = [    
    "*", "/", "%",                // 11.5 Multiplicative operators    
    "+", "-",                     // 11.6 Addtitive operators
    "<<", ">>", ">>>",            // 11.7 Bitwise shift operators
    "<", ">", "<=", ">=",         // 11.8 Relational operators
    "==", "!=", "===", "!==",     // 11.9 Equality operators
    "&", "^", "|",                // 11.10 Binary bitwise operators
    "&&", "||"                    // 11.11 Binary logical operators    
];

for (var op in biops) {
    for (var i=0; i<all.length; ++i) {
        for (var j=0; j<all.length; ++j) {
            write("a["+i+"]("+all[i]+") "+biops[op]+" a["+j+"]("+all[j]+") = " + eval("all[i] " + biops[op] + " all[j];"));            
        }
    }
}
