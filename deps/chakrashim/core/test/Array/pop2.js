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

var testList = new Array(new Number(10) , new Boolean(false));
for (var i=0;i<testList.length;i++) {
    TestPop(InitObject(testList[i]));
}


function popTest2()
{
  var arrObj0 = {};
  var func0 = function(argArr2){
    argArr2[7] = 1;
  }
  var ary = new Array(10);
  ary[1] = 1;
  ary.length = 2;
  ary.pop();
  func0(ary);
  WScript.Echo("ary[1] = " + (ary[1]));
};
popTest2();

Array.prototype[0] = 10;
function popTest3() {
    (function () {
     window = [,];
    }());
    switch (window.pop()) {
    case window:
    }
}
popTest3();
popTest3();