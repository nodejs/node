//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];

function test0() {
    var obj1 = {};
    var IntArr0 = [787917310, 4294967296, 926685325, 104, -1308153184, -1073741824, 1868785301, 1064239984, 693100003, 2147483647, 2147483647, -575755389, -615389387];
    //Snippet:trycatchstackwind.ecs
    function v2201() {
        function v2202() {
            try {
                this.prop1();
            } catch (ex) {
            }
        }
        var v2205 = { prop1: 0.1 };
        v2205.prop1;
        for (var v2206 = 0; v2206 < 1; ++v2206) {
            v2202();
            var v2207 = v2205.prop1;
            v2207 += 1;
            // CSE when used within conditional operator
            var v2208;
            IntArr0 + v2208;
            GiantPrintArray.push(v2207);
        }
    };
    v2201();

    for (var i = 0; i < GiantPrintArray.length; i++) {
        WScript.Echo(GiantPrintArray[i]);
    };
};

// generate profile
test0();
// Run Simple JIT
test0();

// run JITted code
runningJITtedCode = true;
test0();
