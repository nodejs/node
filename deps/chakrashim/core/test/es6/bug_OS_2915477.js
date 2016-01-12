//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var i = 0;
var o = {
    next() {
        return {
            done: i == 1,
            value: [ i++, i ]
        };
    },

    [Symbol.iterator]() {
        return this;
    }
}


o.next = new Proxy(o.next, { });

var m = new Map(o);
if (m.get(0) === 1) {
    WScript.Echo("passed");
} else {
    WScript.Echo("failed");
}
