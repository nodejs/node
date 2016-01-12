//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

const x = 10;
(function f() {
    const x = 20;
    WScript.Echo(x);
})()
WScript.Echo(x);


