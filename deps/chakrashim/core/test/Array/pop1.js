//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v); }

var n = 5;

function InitObject(obj) {
    for (var i=0; i<n; i++) {
        obj[i] = i * i + 1;
    }
    obj.length = n;

    return obj;
}

function TestPop(obj) {
    write(">>> Start pop test for object: " + obj);
    for (var i=0; i<n+2; i++) {
        var x = Array.prototype.pop.call(obj);
        write(i + " iteration. Poped:" + x + " obj.length:" + obj.length);
    }
    write("<<< Stop pop test for object: " + obj);
}

var testList = new Array(new Array() , new Object());
for (var i=0;i<testList.length;i++) {
    TestPop(InitObject(testList[i]));
}