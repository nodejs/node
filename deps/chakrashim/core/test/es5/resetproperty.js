//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testcase1() {

        var arrObj = [];

        Object.defineProperty(arrObj, "length", {
            writable: false
        });

        try {
            Object.defineProperty(arrObj, "length", {
                value: 0
            });
            return true;
        } catch (e) {
            return false;
        }
    }

function testcase2() {

        var arr = [];

        Object.defineProperty(arr, "length", {
            writable: false
        });

        try {
            Object.defineProperties(arr, {
                length: {
                    value: 0
                }
            });
            return true && arr.length === 0;
        } catch (e) {
            return false;
        }
    }

if (testcase1() && testcase2()) {
  WScript.Echo('PASSED');
}
else {
  WScript.Echo('FAILED');
}