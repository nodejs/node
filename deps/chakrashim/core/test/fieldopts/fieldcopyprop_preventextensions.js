//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function () {
    var o = new Object();
    Object.preventExtensions(o);
    o.x = 3;
    WScript.Echo(o.x);
})();
