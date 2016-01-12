//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("CompoundStringUtilities.js", "self");

CompoundString.createTestStrings(); // call twice so that it is jitted the second time
var strings = CompoundString.createTestStrings();
for(var i = 0; i < strings.length; ++i)
    WScript.Echo(i + ": " + strings[i]);
