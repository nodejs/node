//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = { x: 20, y: 30 };
Object.freeze(a);
delete a.x;
a.x

var a = { x: 20, y: 30 };

WScript.Echo("ok");