//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var crossSiteGlo = WScript.LoadScriptFile("es5array_crosssite.js", "samethread");

var obj = {};
Object.defineProperty(obj, "1", { value: "const", writable: false });

crossSiteGlo.test_obj_proto(obj);

