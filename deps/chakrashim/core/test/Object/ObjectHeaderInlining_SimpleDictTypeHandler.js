//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(arg0, arg1) {
    this.prop0 = arg0;
}

var obj = new test(1,2);
Object.defineProperty(obj, "a", {value : 37,
                               writable : true});
obj[0] = 10; //Adding this should move the ObjectHeaderInlined Elements to AUX slots

WScript.Echo(obj.prop0);
WScript.Echo(obj[0]);