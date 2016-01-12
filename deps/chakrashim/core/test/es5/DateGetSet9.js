//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var d = new Date();

d.setDate(12345678);
d.setTime(456789);

WScript.Echo("toISOString : " + d.toISOString());
WScript.Echo("toJSON : " + d.toJSON());


// Test NaN Date value
d = new Date(Number.NaN);
try
{
   d.toISOString();
} catch(e) {
    WScript.Echo("NaN Date toISOString: " + e.name + " : " + e.message);
}
WScript.Echo("NaN Date toJSON:: " + d.toJSON());

//
// Test Infinity Date value
//
d = new Date(Infinity);
try {
    d.toISOString();
} catch(e) {
    WScript.Echo("Infinity Date toISOString : " + e.name + " : " + e.message);
}
WScript.Echo("Infinity Date toJSON : " + d.toJSON());

//
// Test Date.prototype.toJSON transfered to an object but toISOString is not callable
//
d = {
    toISOString: 1,
    toJSON: Date.prototype.toJSON
};
try {
    d.toJSON();
} catch(e) {
    WScript.Echo("Object toISOString not callable : " + e.name + " : " + e.message);
}

//
// Test Date.prototype.toJSON transfered to an object
//
d = {
    toISOString: function() {
        return "Fake JSON : Object";
    },
    toJSON: Date.prototype.toJSON
};
WScript.Echo("Object toJSON : " + d.toJSON());

//
// Test Date.prototype.toJSON transfered to String
//
String.prototype.toISOString = function() {
    return "Fake JSON : " + this;
};
String.prototype.toJSON = Date.prototype.toJSON;
d = "String";
WScript.Echo("String toJSON : " + d.toJSON());

//
// Test Date.getYear -- ES5 spec B.2.4
// 
WScript.Echo("getYear 2000: " + new Date("January 1 2000").getYear());
WScript.Echo("getYear 1899: " + new Date("January 1 1899").getYear());

Object.defineProperty(Date.prototype, "valueOf", {get: function() {WScript.Echo("get fired");}});
var d = new Date();
d.toJSON();
