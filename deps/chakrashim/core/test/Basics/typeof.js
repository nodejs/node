//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("typeof (new String()) : " + typeof (new String("")));
WScript.Echo("typeof () : " + typeof (""));

WScript.Echo("typeof (new Boolean(false)) : " + typeof (new Boolean(false)));
WScript.Echo("typeof (false) : " + typeof (false));

WScript.Echo("typeof (new Number(0)) : " + typeof (new Number(0)));
WScript.Echo("typeof (0) : " + typeof (0));


WScript.Echo("typeof (new Number(12345.678)) : " + typeof (new Number(12345.678)));
WScript.Echo("typeof (12345.678) : " + typeof (12345.678));

function f() {
    function g() { }
    return g;
}

WScript.Echo("typeof function : " + typeof (f));
WScript.Echo("typeof function returning function : " + typeof (f()));

function exc() {
    try {
        WScript.Echo(q);
    }
    catch (e) {
        WScript.Echo("Caught JS error on undefined var");
        WScript.Echo(typeof (q));
    }
}
exc();

var x = {};
WScript.Echo("typeof empty obj : " + typeof (x));
WScript.Echo("typeof obj : " + typeof (new Object()));

var y = [];
y[0] = 0;
WScript.Echo("typeof array element with number : " + typeof (y[0]));
WScript.Echo("typeof undef element array : " + typeof (y[1]));
WScript.Echo("typeof array : " + typeof (y));

var verr = new Error("aaa");
WScript.Echo("typeof (err) : " + typeof (verr));

var vDate = new Date(123);
WScript.Echo("typeof ( new Date) : " + typeof (vDate));

WScript.Echo("typeof (new Array()) : " + typeof (new Array()));

var regx = /abc/;
WScript.Echo("typeof(regex) : " + typeof (regx));

var s;
var map = {};
function CEvent() {
    do { 
    } while(typeof (s = map.x) != "undefined");
}
CEvent();
