//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var CrossContextSpreadTestFunction = this.WScript.LoadScriptFile("CrossContextSpreadFunction.js", "samethread");
var CrossContextSpreadArguments = this.WScript.LoadScriptFile("CrossContextSpreadArguments.js", "samethread");

CrossContextSpreadTestFunction.foo(...CrossContextSpreadArguments.a);