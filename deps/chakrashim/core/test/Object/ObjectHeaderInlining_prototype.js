//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var addProperty = function (o) {
    o.prop1 = 1;
};

function TwoProperty(arg0, arg1) {
    this.prop0 = arg0;
    this.prop2 = arg1;
}

obj6 = new TwoProperty();
Object.create(obj6);
addProperty(obj6);

WScript.Echo("PASSED");
