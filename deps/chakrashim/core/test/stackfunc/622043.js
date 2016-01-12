//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var runningJITtedCode = false;

function test0() {
    var func0 = function () {
        var v3877 = runningJITtedCode;

        function v3878() {

            function v3879() {

                return v3879;
            }
            if (v3877)
                return v3879();
        }
        var v3880 = v3878();
    }
    var func2 = function () {
        func0();
    }
    func0.call(func2.call());
};

// generate profile
test0();

// run JITted code
runningJITtedCode = true;
test0();

WScript.Echo('pass');