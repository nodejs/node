//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -force:fieldhoist -off:inlinegetters -off:fixedmethods -mic:1 -msjrc:1

var obj1 = {};
Object.defineProperty(obj1, "prop0", {
        get: function(){return this._prop0;},
        set: function(a){this._prop0 = a;},
        configurable: true
    });

arrObj0 = [];
var ret;
function foo(arrObj0, obj1)
{
    arrObj0.length;
    obj1.prop0 = 1;
    for (var i = 0;i < 3; i++)
    {
        obj1.prop0 = i;
        ret = obj1.prop0;
    }
}

foo(arrObj0, obj1);
WScript.Echo(ret);

foo(arrObj0, obj1);
WScript.Echo(ret);

foo(arrObj0, obj1);
WScript.Echo(ret);
