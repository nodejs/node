//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(i) {
    while (true) {
        if (i > 1000 * 1000)
            break;
        if (i == 1) {
            var x = i;
            if (!x.q) ++i;
            x.x;
        }
        i++;
    }
}
foo(1);
WScript.Echo('pass');
