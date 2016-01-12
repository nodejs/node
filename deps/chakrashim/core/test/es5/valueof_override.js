//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



var valueOfAccessed = "FAIL";
Date.prototype.valueOf = function () {
    valueOfAccessed = "PASS";
    return " ";
}
var dateObj = new Date(2010, 11, 31, 0, 0, 0, 0);
var dToJSON = dateObj.toJSON();
    
WScript.Echo(valueOfAccessed);

