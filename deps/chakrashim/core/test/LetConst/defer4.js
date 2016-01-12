//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Blue bug 241839
function val() {
    return 1;
}

function testSwitch1() {
    switch (val()) {
    case 1:
        let z = 10; // No error
        z++;
        break;
    case 2:
        let y = 1; // No error
        y++;
        break;
    }
}

function testSwitch2() {
    switch (val()) {
    case 1:
        switch (val()) {
        default:
            let a = 1; // No error
            break;
        }
    }
}

function testSwitch3() {
    var a = 1;
    while (a)
        switch (val()) {
        default:
            let b = 2; // No error
            ++b;
            a = 0;
            break;
        }
}

testSwitch1();
testSwitch2();
testSwitch3();

// Reduced hang found during development.
(function () { try { eval(
    "switch (Math()) { \
    default: \
        function func4() { \
            switch (--e) { \
            } \
        } \
    }"
); } catch (e) { WScript.Echo(e) }})();

WScript.Echo('Pass');