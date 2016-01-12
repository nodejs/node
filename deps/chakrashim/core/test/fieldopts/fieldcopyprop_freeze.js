//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function () {
    var o = new Object();
    o.x = 4;
    Object.freeze(o);
    o.x = 3;
    WScript.Echo(o.x);
})();
