//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    function func9() {
    }
    function _array2iterate(_array2tmp) {
        for (var _array2i in _array2tmp) {
            if (Array) {
                _array2iterate(_array2tmp[_array2i]);
                ({ prop5: this.undefined });
                obj11 = func9();
            }
        }
    }
    _array2iterate([[[[]]]]);
}
test0();
test0();

WScript.Echo('pass');
