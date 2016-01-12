//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    function getp(o) {
        return o.p;
    }

    function setp(o) {
        o.p = 1;
    }

    function c1() { }
    c1.prototype = { p: 0 };
    function c2() { }

    var p1 = new c1();
    c2.prototype = p1;
    var o = new c2();

    var ptemp = new c1();
    c2.prototype = ptemp;
    var p2 = new c2();

    setp(ptemp);
    WScript.Echo(getp(o));
    setp(p1);
    WScript.Echo(getp(o));
}
test0();
