//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
This file is executed from fabs1.js

fabs from ucrtbase.dll doesn't restore the FPU Control word correctly when passed a NaN. 
This is exposed if we WScript.LoadScriptFile() code with Math.Abs(NaN) in it. 
Causing an assertion failure in SmartFPUControl. The change special-handles NaN without calling fabs
*/

function testOp(av) {
    return Math.abs(av);
}

WScript.Echo(testOp(123.334) === 123.334);
WScript.Echo(testOp(-123.334) === 123.334);
WScript.Echo(isNaN(testOp(NaN)));
WScript.Echo(isNaN(testOp(-NaN)));
WScript.Echo(testOp(Infinity) === Infinity);
WScript.Echo(testOp(-Infinity) === Infinity);
WScript.Echo(testOp(0.0) === 0.0);
WScript.Echo(testOp(-0.0) === 0.0);

