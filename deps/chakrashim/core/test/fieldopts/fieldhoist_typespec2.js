//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// - 'a' is considered a field since it's used by a nested function
// - 'a' is hoisted out of the outermost loop
// - At 'a |= 1', the hoisted stack sym for 'a' is int-specialized
// - At the first 'test0a()", 'a' is killed but we are lazy on killing the corresponding stack sym and the field value
// - The use of 'a' in 'i = a' in the innermost loop is hoistable out of the innermost loop, this is the first use of 'a' after
//   it was killed, and whether it's hoisted out of the innermost loop or not, the specialized stack sym and the field value are
//   no longer valid at the landing pad of the innermost loop. So, they must be killed at this point in the loop prepass.
function test0() {
    var a = 1;
    var o = [0];
    for(var i = 0; (a |= 1) && i < 1; ++i) {
        test0a();
        for(var j = 0; j < 1; ++j) {
            for(var k = 0; k < 1; ++k) {
                i = a;
            }
            test0a();
        }
        i = a;
    }

    function test0a() { a; }
};
test0();

WScript.Echo("pass");
