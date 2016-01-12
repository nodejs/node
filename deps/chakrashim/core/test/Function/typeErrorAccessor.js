//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function printDesc(d) {
    write(d);
    write(d.set);
    write(d.get);

    var s = "V:" + d.value + ", W:" + d.writable + ", E:" + d.enumerable + ", C:" + d.configurable;
    s += ", get:" + d.hasOwnProperty('get') + ", set:" + d.hasOwnProperty('set');

    write(s);
}

function f() { return true; };
var g = f.bind();

var callerDesc = Object.getOwnPropertyDescriptor(g.__proto__, 'caller');
var getter = callerDesc.get;

write("***************** getter ***************** ");
write("length = " + getter.length);

printDesc(Object.getOwnPropertyDescriptor(getter, 'length'));

write("***************** g.caller ***************** ");
printDesc(callerDesc);

write("***************** g.arguments ***************** ");
printDesc(Object.getOwnPropertyDescriptor(g.__proto__, 'arguments'));

write("***************** try to set/get the caller/arguments *****************");
try {
    g.caller = {};
    write("fail1");
} catch (e) {
    write("Set caller passed");
}

try {
    write(g.caller);
    write("fail2");
} catch (e) {
    write("Get caller passed");
}

try {
    g.arguments = {};
    write("fail3");
} catch (e) {
    write("Set arguments passed");
}

try {
    write(g.arguments);
    write("fail4");
} catch (e) {
    write("Get arguments passed");
}
