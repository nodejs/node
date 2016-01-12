//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var all = [ 0, 10.2, 1, -10, "1", "0xa", "007",
            "hello", "helloworld", "hel",
            [1,2,3], new Array(10), new Object(), 
            undefined, null,
            false, true,
            Number.MAX_VALUE, Number.MIN_VALUE, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN
          ];

for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        write("a[" + i + "] <  a[" + j + "] = " + (all[i] < all[j]));
        write("a[" + i + "] >  a[" + j + "] = " + (all[i] > all[j]));
        write("a[" + i + "] <= a[" + j + "] = " + (all[i] <= all[j]));
        write("a[" + i + "] >= a[" + j + "] = " + (all[i] >= all[j]));
    }
}