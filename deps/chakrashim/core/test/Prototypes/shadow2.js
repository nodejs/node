//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var Failed = 0;

function Fail() {
    WScript.Echo("*** FAILED ***\n");
    Failed++;
}

function check(o, v) {
    if (o.value != v) Fail();
}

function first()
{
}

first.value = 1;

function second()
{
}
second.prototype = first;

function third()
{}

third.prototype = new second();

var obj1 = new third();

obj1.foo = 45;

delete obj1.foo;  // Force to dictionary

check(obj1, 1);

third.prototype.value = 2;

check(obj1, 2);

delete third.prototype.value;

check(obj1, 1);

if (Failed == 0) {
    WScript.Echo("Pass\n");
}
