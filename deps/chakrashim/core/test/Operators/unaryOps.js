//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo() {}

var all = [ undefined, null, //1
            true, false, // 3
            Boolean(true), Boolean(false), // 5
            new Boolean(true), new Boolean(false), //7
            NaN, +0, -0, 0, 0.0, -0.0, +0.0, // 14
            1, 10, 10.0, 10.1, -1,  // 19
            -10, -10.0, -10.1,      // 22
            Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, //27
            new Number(NaN), new Number(+0), new Number(-0), new Number(0), // 31
            new Number(0.0), new Number(-0.0), new Number(+0.0),  //34
            new Number(1), new Number(10), new Number(10.0), new Number(10.1), new Number(-1), // 39
            new Number(-10), new Number(-10.0), new Number(-10.1), // 42
            new Number(Number.MAX_VALUE), new Number(Number.MIN_VALUE), new Number(Number.NaN), // 45
            new Number(Number.POSITIVE_INFINITY), new Number(Number.NEGATIVE_INFINITY), // 47
            "", "hello", "hel" + "lo", // 50
            String(""), String("hello"), String("h" + "ello"),
            new String(""), new String("hello"), new String("he" + "llo"),
            new Object(), new Object(),
            [1,2,3], [1,2,3],
            new Array(3), Array(3), new Array(1,2,3), 
            Array(1),
            foo,
            Math.pow(2,29)-2,Math.pow(2,29)-1,Math.pow(2,29),Math.pow(2,29)+1,Math.pow(2,29)+2,
            Math.pow(2,30)-2,Math.pow(2,30)-1,Math.pow(2,30),Math.pow(2,30)+1,Math.pow(2,30)+2,
            Math.pow(2,31)-2,Math.pow(2,31)-1,Math.pow(2,31),Math.pow(2,31)+1,Math.pow(2,31)+2,
            Math.pow(2,32)-2,Math.pow(2,32)-1,Math.pow(2,32),Math.pow(2,32)+1,Math.pow(2,32)+2,
            -(Math.pow(2,29)-2),-(Math.pow(2,29)-1),-(Math.pow(2,29)),-(Math.pow(2,29)+1),-(Math.pow(2,29)+2),
            -(Math.pow(2,30)-2),-(Math.pow(2,30)-1),-(Math.pow(2,30)),-(Math.pow(2,30)+1),-(Math.pow(2,30)+2),
            -(Math.pow(2,31)-2),-(Math.pow(2,31)-1),-(Math.pow(2,31)),-(Math.pow(2,31)+1),-(Math.pow(2,31)+2),
            -(Math.pow(2,32)-2),-(Math.pow(2,32)-1),-(Math.pow(2,32)),-(Math.pow(2,32)+1),-(Math.pow(2,32)+2)
          ];

var uops = [ 
    // commented pre-incr/decr and moved out of eval due to bug 407   
    "typeof", /*"++", "--",*/ "+", "-", "~", "!", "void"
];

var val = 0;
for (var op in uops) {
    for (var i=0; i<all.length; ++i) {
        val = all[i];
        write(uops[op]+ " a["+i+"]("+all[i]+") = " + eval(uops[op] + " val;") + ". Value: " + val);            
    }
}

// Pre/post Incr/Decr
for (var i=0; i<all.length; ++i) {
    val = all[i];
    write(" ++a["+i+"]("+all[i]+") = " + (++val) + ". Value: " + val);
    
    val = all[i];
    write(" --a["+i+"]("+all[i]+") = " + (--val) + ". Value: " + val);
    
    val = all[i];
    write(" a["+i+"]("+all[i]+")++ = " + (val++) + ". Value: " + val);
    
    val = all[i];
    write(" a["+i+"]("+all[i]+")-- = " + (val--) + ". Value: " + val);    
}