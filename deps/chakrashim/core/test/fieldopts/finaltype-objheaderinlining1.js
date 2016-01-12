//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var o = { p0: 0 };
    test0a(o);
    test0a.call(o, o);

    function test0a(o) {
        o.p0 = 0;
        this.p1 = 0;
        ++o.p0;
        o.p2 = 0;
    };
}
test0();

function test1() {
    var o = new test1Construct();
    test1a(o);
    test1a.call(o, o);

    function test1a(o) {
        o.p0 = 0;
        this.p1 = 0;
        ++o.p0;
        o.p2 = 0;
    };
}
function test1Construct() {
    this.p0 = 0;
}
test1();

WScript.Echo('pass');
