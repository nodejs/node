//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Regress Win8 854057
//

function test(num)
{
    var arr = new Array(num);

    // Generate ES5ArrayTypeHandler index map in order
    for(var i = 0; i < num; ++i)
    {
        Object.defineProperty(arr, i, {
           value: i,
           enumerable: true,
           writable: false,
           configurable: true
        });
    }

    // Enumerator triggers generating index list
    var i = 0;
    for (var p in arr) {
        var value = arr[p];
        if (i++ > 5) {
            break;
        }
    }
}

function oos() {
    try {
        oos();
    } catch(e) {
        // We just got OOS, now we have limited stack
        test(1000000);
    }
}

oos();

// Good if we haven't hit hard OOS
WScript.Echo("pass");
