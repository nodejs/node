//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(a) {
    return a + 1.1;
}
echo(test0(1.1));

function test1() {
    return Math.sin(1.1) + 1.1;
}
echo(test1());

function test2(a) {
    a[0] = 1.1;
}
var a = new Float64Array(1);
test2(a);
echo(a[0]);

function test3(a) {
    return a | 0;
}
echo(test3(1.1));

echo(Math.abs(-1.1));

function echo(n) {
    WScript.Echo(Math.round(n * 100));
}
