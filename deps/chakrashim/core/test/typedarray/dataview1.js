//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
    this.WScript.LoadScriptFile("util.js");
}

function test1() {
    var intArray = Array(0x100); //[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var arrayBuffer = (new Uint32Array(intArray)).buffer;
    var viewStart = 0;
    var viewLength = arrayBuffer.byteLength;
    var view = new DataView(arrayBuffer, viewStart, viewLength);
    for (var i = 0; i <= 8; i++) {
        try {
            WScript.Echo('view.getUint32(-' + i + '): 0x' + view.getUint32(-i).toString(16));
        } catch (e) {
            WScript.Echo(e.message);
        }
    }
    for (var i = 0; i <= 8; i++) {
        try {
            WScript.Echo('view.setUint32(-' + i + '): 0x' + view.setUint32(-i, 10).toString(16));
        } catch (e) {
            WScript.Echo(e.message);
        }
    }
}

function test2() {
    var arrayBuffer = new ArrayBuffer(10);

    try{
        var view1 = new DataView(arrayBuffer, undefined);
    } catch (e) {
        if (e instanceof RangeError) {
            if(e.message !== "DataView constructor argument byteOffset is invalid"){
                WScript.Echo('FAIL');
            }
        } else {
            WScript.Echo('FAIL');
        }
    }

    try{
        var view2 = new DataView(arrayBuffer, 1.5);
    } catch (e) {
        if (e instanceof RangeError) {
            if (e.message !== "DataView constructor argument byteOffset is invalid") {
                WScript.Echo('FAIL');
            }
        } else {
            WScript.Echo('FAIL');
        }
    }
    WScript.Echo('PASS');
}

function test3() {
    var v1 = new DataView(new ArrayBuffer(), 0, 0);
    var v2 = new DataView(new ArrayBuffer(1), 1, 0);
}

function test4() {
    var arrayBuffer = (new Uint32Array([0, 1, 2, 3])).buffer;
    var view1 = new DataView(arrayBuffer);
    var view2 = new DataView(arrayBuffer, 0);
    var view3 = new DataView(arrayBuffer, 0, undefined);
    if ((view1.byteLength === view2.byteLength) && (view2.byteLength === view3.byteLength)) {
        WScript.Echo('PASS');
        for (var i = 0; i < 4; i++) {
            if ((view1.getUint32(i) === view2.getUint32(i)) && (view2.getUint32(i) === view3.getUint32(i))) {
                WScript.Echo('PASS');
            } else {
                WScript.Echo('FAIL');
                WScript.Echo(view1.getUint32(i));
                WScript.Echo(view2.getUint32(i));
                WScript.Echo(view3.getUint32(i));
            }
        }
    } else {
        WScript.Echo('FAIL');
        WScript.Echo(view1.byteLength);
        WScript.Echo(view2.byteLength);
        WScript.Echo(view3.byteLength);
    }
}

function test5() {
    assert.throws(
        function () { var dv = new DataView(new ArrayBuffer(0x100000), 1, 0xffffffff); },
        RangeError,
        "DataView constructor argument byteLength is invalid");
}

test1();
test2();
test3();
test4();
test5();