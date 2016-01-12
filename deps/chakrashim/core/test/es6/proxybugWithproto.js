//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -nonative
//  -maxsimplejitruncount:1 -maxinterpretcount:1 
var arrObj0 = {};
var uic8 = new Uint8ClampedArray(256);
var proxyHandler = {};
for (var _strvar28 in uic8) {
    arrObj0.__proto__ = Array;
    switch ('(') {
        case '(':
        case 1:
            arrObj0 = new Proxy(arrObj0, proxyHandler);
    }
}
print('PASS');