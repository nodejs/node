//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

String.prototype.toString = function () {
    return "PASS";
}
var mySpace = new String("FAIL");
WScript.Echo(mySpace.substr(0,4));


