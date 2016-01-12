//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var re = /^on([A-Z])/; 
var inputs = ["onClick"]; 
var result ;
for( var i = 0; i < inputs.length; i++) { 
    result = re.exec(inputs[i]); 
}
WScript.Echo(result.toString());
