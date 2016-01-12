//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var IntArr1 = new Array();
prop1 = 1;
IntArr1[0] = 1;
IntArr1[1] = 1;
IntArr1[3] = 2147483647;
IntArr1[2] = 1;
IntArr1 = IntArr1.concat(prop1);
// Native Int Array change to Native Float Array in the middle of forEach
IntArr1.forEach(function (element, index) {
    WScript.Echo(IntArr1[index]++);
    WScript.Echo(element);
});

