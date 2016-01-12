//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(f, f2) {
    return f = f(), f2 = f2();
}
test0(f, f);

function test1(f, f2) {
    return f = new f(), f2 = new f2();
}
test1(f, f);

function test2(f, f2) {
    return f = new f(0), f2 = new f2(0);
}
test2(f, f);

function f() {
    return 0;
}

WScript.Echo("pass");
