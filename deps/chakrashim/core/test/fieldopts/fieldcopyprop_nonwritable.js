//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

for (var b in [1]) { NaN+=b; }
WScript.Echo(NaN);

var x = 4;
Object.defineProperty(this, "x", { writable: false });
x = 3;
WScript.Echo(x);
