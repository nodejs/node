//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x) }

// Verify that side-effects happen in the right order w.r.t. evaluation of operands.
(function () {
    //CHECK#1 
    var NUMBER = 0;
    if ((NUMBER = Number, "MAX_VALUE") in NUMBER !== true) {
        print('#1: var NUMBER = 0; (NUMBER = Number, "MAX_VALUE") in NUMBER === true');
    }

    //CHECK#2
    var max_value = "MAX_VALUE";
    if (max_value in (max_value = "none", Number) !== true) {
        print('#2: var max_value = "MAX_VALUE"; max_value in (max_value = "none", Number) === true');
    }
})();

print('Passed');
