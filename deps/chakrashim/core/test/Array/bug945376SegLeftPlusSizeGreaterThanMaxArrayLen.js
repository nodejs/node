//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = [];
a[4294967290] = 4;
a.splice(0,0,0,1); //length grows by 2
a.push(4);
WScript.Echo("PASS");