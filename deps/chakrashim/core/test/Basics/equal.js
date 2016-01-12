//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo() {}

var all = [ 0, 10.2, 1, -10, "1",
            "hello", undefined, true,false, new Array(10),
            null, Number.MAX_VALUE, Number.POSITIVE_INFINITY
          ];
    
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        write("a[" + i + "] == a[" + j + "] = " + (all[i] == all[j]));
    }
}