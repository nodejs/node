//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test field hoisting cases that should strip bailouts in the loop header.

// Field load hoisted from within the inner do-while should have bailout removed.
function test1() {
    var obj4 = 1;
    var __loopvar2 = 0;
    while ((1) && __loopvar2 < 3) {
        __loopvar2++;
        var obj6 = 1;
        for (var __loopvar3 = 0; obj4.length < (1.1) && __loopvar3 < 3; obj4.length++ + __loopvar3++) {
        }
        var __loopvar3 = 0;
        do {
            __loopvar3++;
            p0 = ui8_1[(1) % 256];
        } while (((obj6.length++)) && __loopvar3 < 3)
    }
}

// Field load hoisted from within the inner do-while should have BailOutExpectingObject,
// which should not clobber implicit call bit set by the load of (undefined) ui8.
function test2() {
    var obj4 = 1;
    var __loopvar2 = 0;
    while ((1) && __loopvar2 < 3) {
        __loopvar2++;
        var obj6 = {};
        for (var __loopvar3 = 0; obj4.length < (1.1) && __loopvar3 < 3; obj4.length++ + __loopvar3++) {
        }
        var __loopvar3 = 0;
        do {
            __loopvar3++;
            p0 = ui8_2[(1) % 256];
            obj6.spam;
        } while (((obj6.length++)) && __loopvar3 < 3)
    }
}

try {
    test1();
}
catch(e1) {
    WScript.Echo(e1.message);
}

try {
    test2();
}
catch(e2) {
    WScript.Echo(e2.message);
}

(function(){
  var obj1 = new Object();
  var obj4 = new Object();
  (function(){
    var __loopvar2 = 0;
    do {
      __loopvar2++;
      e = ((obj4.prop0 * (obj4.prop1 -= -2147483648)) - (obj1.prop0++ ));
    } while((1.1) && __loopvar2 < 3)
  })();
})();
