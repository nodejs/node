//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("-------Named Type Test Start------------------");
var obj = JSON.parse('{"2a":"foo"}', function(key, value){
    WScript.Echo(key, ':', value);
    return value;
});
WScript.Echo(obj["2a"]);
WScript.Echo("-------Named Type Test End--------------------");
WScript.Echo("-------Simple Numeral Type Test Start---------");
var obj2 = JSON.parse('{"2":"foo"}', function(key, value){
    WScript.Echo(key, ':', value);
    return value;
});
WScript.Echo(obj2["2"]);
WScript.Echo("-------Simple  Numeral Type Test End----------");
WScript.Echo("-------Complex Numeral Type Test Start--------");
var obj3 = JSON.parse('{"3":{"1":"foo"}}', function(key, value){
     WScript.Echo(key, ':', value);
    return value;
});
WScript.Echo(obj3["3"]);
WScript.Echo("-------Complex Numeral Type Test End----------");
