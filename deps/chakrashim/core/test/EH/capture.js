//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f1(x) {
    try {
        throw 'catch';
    }
    catch (x) {
        var f2 = function () {
            WScript.Echo(x);
        }
        f2();
        function f3() {
            WScript.Echo(x);
            try {
                throw 'catch2';
            }
            catch (y) {
                f2();
                var f4 = function () {
                    WScript.Echo(x, y);
                }
                function f5() {
                    WScript.Echo(x, y);
                }
            }
            f4();
            f5();
        }
        f3();
    }
}
y = 'y';
f1('param');

function f10(){
    var ex = 'Carey Price';
    try {
        throw 1;
    } catch(ex) {
        try {
            throw 2;
        } catch(ex) {
            function f11 (){};
            function f12 (){ WScript.Echo(ex); };
        }
    }
    f12();
};
f10();
