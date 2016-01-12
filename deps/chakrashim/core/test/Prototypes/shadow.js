//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var Failed = 0; 

function Fail() {
    WScript.Echo("*** FAILED ***\n");
    Failed++;
}

function check(o, v)
{
    o.value(v);
}

function first()
{
}

function isFirst(v) { if (v != 1) Fail(); }
function isSecond(v) { if (v != 2) Fail(); }

first.value = isFirst;

function second()
{
}
second.prototype = first;

function third()
{}

third.prototype = new second();

var obj1 = new third();

check(obj1, 1);

third.prototype.value = isSecond;

check(obj1, 2);

delete third.prototype.value;

check(obj1, 1);

if (Failed == 0) {
    WScript.Echo("Pass\n");
}
