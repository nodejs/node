//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var test = false;

function OneProperty(arg0) {
    this.prop0 = arg0;
}

var AddMoreProperties = function () {
    
    obj = new OneProperty(3);
    
    if(test){ WScript.Echo("A") };
    
    obj.prop1 = 1;
    obj.prop2 = 4;
    
    WScript.Echo(obj.prop0);
    WScript.Echo(obj.prop1);
    WScript.Echo(obj.prop2);
    obj[0];
};


AddMoreProperties();
AddMoreProperties();
AddMoreProperties();

