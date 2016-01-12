//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// 'b * (1 ? a : 0.1)' is float-preffed and 'c * 3' overflows. The 'Add_A' of the resulting values is executed in the
// interpreter and it requires that upon bailout, the float-preffed result of the left side had been converted to var. This test
// verifies that the float pref recovery happens correctly upon bailout (if it does not happen, it will likely cause a crash
// upon reading random memory).
function test0(c) {
    var a = 1;
    function test0a() {
        a;
    }
    b = 1;
    do {
        b * (1 ? a : 0.1) + c * 3;
    } while(false);
}
test0(0x3fffffff);
